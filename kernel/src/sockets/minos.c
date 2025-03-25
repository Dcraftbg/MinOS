#include "minos.h"
#include <assert.h>
#include <mem/slab.h>
#include <minos/status.h>
#include <fs/tmpfs/tmpfs.h>
#include "log.h"
// TODO: Maybe round up N to the nearest page. I'm not sure
static intptr_t minos_cp_reserve_exact(MinOSConnectionPool* cp, size_t n) {
    void* old_items = cp->items;
    cp->items = kernel_malloc(n * sizeof(*cp->items));
    if(!cp->items) {
        cp->items = old_items;
        return -NOT_ENOUGH_MEM;
    }
    cp->cap = n;
    if(old_items) kernel_dealloc(old_items, n * sizeof(*cp->items));
    return 0;
}
static intptr_t minos_cp_reserve(MinOSConnectionPool* cp, size_t extra) {
    if(cp->len + extra >= MINOS_SOCKET_MAX_CONNECTIONS) return -LIMITS;
    intptr_t e = 0;
    if(cp->len + extra > cp->cap) e = minos_cp_reserve_exact(cp, cp->cap * 2 + extra);
    return e;
}
static void minos_cp_free(MinOSConnectionPool* cp) {
    kernel_dealloc(cp->items, cp->cap * sizeof(*cp->items));
    cp->items = NULL;
    cp->cap = 0;
    cp->len = 0;
}
static intptr_t minos_data_reserve_at_least(MinOSData* dp, size_t n) {
    void* old_addr = dp->addr;
    dp->addr = kernel_malloc(n * sizeof(*dp->addr));
    if(!dp->addr) {
        dp->addr = old_addr;
        return -NOT_ENOUGH_MEM;
    }
    dp->cap = PAGE_ALIGN_UP(n) / sizeof(*dp->addr);
    if(old_addr) kernel_dealloc(old_addr, n * sizeof(*dp->addr));
    return 0;
}
static Cache* minos_socket_cache = NULL;
void minos_socket_init_cache(void) {
    assert(minos_socket_cache = create_new_cache(sizeof(MinOSSocket), "MinOSSocket"));
}
static MinOSSocket* minos_socket_new(void) {
    MinOSSocket* socket = cache_alloc(minos_socket_cache);
    if(!socket) return NULL;
    memset(socket, 0, sizeof(*socket));
    return socket;
}
static bool contains_null(const char* str, size_t n) {
    for(size_t i = 0; i < n; ++i) {
        if(str[i] == '\0') return true;
    }
    return false;
}
// Generic operations:
static intptr_t minos_bind(Inode* sock, struct sockaddr* addr, size_t addrlen) {
    if(addrlen != sizeof(struct sockaddr_minos)) return -SIZE_MISMATCH;
    if(addr->sa_family != AF_MINOS) return -ADDR_SOCKET_FAMILY_MISMATCH;
    if(!contains_null(((MinOSSocket*)sock->priv)->addr, SOCKADDR_MINOS_PATH_MAX)) return -INVALID_PATH;
    // TODO: verify the path is null terminated
    memcpy(((MinOSSocket*)sock->priv)->addr, ((struct sockaddr_minos*)addr)->sminos_path, SOCKADDR_MINOS_PATH_MAX);
    return vfs_socket_create_abs(((MinOSSocket*)sock->priv)->addr, iget(sock));
}
// Client Ops
static intptr_t minos_write(Inode* sock, const void* data, size_t size, off_t _off) {
    (void)_off;
    MinOSClient* mc = &((MinOSSocket*)sock->priv)->client;
    mutex_lock(&mc->data_lock);
    intptr_t e;
    if(mc->data.len == MINOS_SOCKET_MAX_DATABUF) {
        mutex_lock(&mc->data_lock);
        return 0;
    }
    size_t n = size;
    if(mc->data.len + n > MINOS_SOCKET_MAX_DATABUF) {
        n = MINOS_SOCKET_MAX_DATABUF - mc->data.len;
    }
    if((e=minos_data_reserve_at_least(&mc->data, n)) < 0) {
        mutex_lock(&mc->data_lock);
        return e;
    }
    memcpy(mc->data.addr + mc->data.len, data, n);
    mc->data.len += n;
    mutex_unlock(&mc->data_lock);
    return n;
}
static intptr_t minos_read(Inode* sock, void* data, size_t size, off_t _off) {
    (void)_off;
    MinOSClient* mc = &((MinOSSocket*)sock->priv)->client;
    mutex_lock(&mc->data_lock);

    if(mc->data.len == 0 && mc->closed) return -CONNECTION_TERM;
    if(mc->data.len == 0) {
        mutex_unlock(&mc->data_lock);
        return -WOULD_BLOCK;
    }
    if(mc->data.len < size) size = mc->data.len;
    memcpy(data, mc->data.addr, size);
    mc->data.len -= size;
    memmove(mc->data.addr, mc->data.addr + size, mc->data.len);
    mutex_unlock(&mc->data_lock);
    return size;
}
static void minos_client_cleanup(Inode* sock) {
    MinOSClient* client = &((MinOSSocket*)sock->priv)->client;
    // TODO: use exchange or set_flag_and_test.
    if(!client->closed) {
        mutex_lock(&client->data_lock);
        kernel_dealloc(client->data.addr, client->data.cap * sizeof(*client->data.addr));
        mutex_unlock(&client->data_lock);
        cache_dealloc(minos_socket_cache, client);
    }
    client->closed = true;
}
// FIXME: close
static InodeOps minos_client_ops = {
    .read = minos_read,
    .write = minos_write,
    .cleanup = minos_client_cleanup,
};
// Server Ops
static intptr_t minos_accept(Inode* sock, Inode* result, struct sockaddr* addr, size_t *addrlen) {
    MinOSServer* server = &((MinOSSocket*)sock->priv)->server;
    rwlock_begin_write(&server->pending.lock);
    if(server->pending.len == 0) {
        rwlock_end_write(&server->pending.lock);
        return -WOULD_BLOCK;
    }
    MinOSClient* client = server->pending.items[0];
    result->priv = client;
    result->ops = &minos_client_ops;
    server->pending.len--;
    memmove(server->pending.items, server->pending.items + 1, server->pending.len * sizeof(*server->pending.items));
    client->pending = false;
    rwlock_end_write(&server->pending.lock);
    return 0;
}
// FIXME: close
static InodeOps minos_server_ops = {
    .accept = minos_accept
};
// Unspecified operations.
static intptr_t minos_listen(Inode* sock, size_t n) {
    intptr_t e;
    MinOSServer* server = &((MinOSSocket*)sock->priv)->server;
#if 0 // <- Debug testing I guess
    if(strlen(server->addr) == 0) {
        // TODO: better error status for this:
        kwarn("minos socket: called listen before bind!");
        return -INVALID_TYPE;
    }
#endif
    if(n > MINOS_SOCKET_MAX_CONNECTIONS) {
        kwarn("minos socket: called listen with n (%zu) > %zu", n, MINOS_SOCKET_MAX_CONNECTIONS);
        return -LIMITS;
    }

    rwlock_begin_write(&server->pending.lock);
    if(server->pending.len > n) {
        kwarn("minos socket: called listen but backlog is bigger");
        rwlock_end_write(&server->pending.lock);
        return -LIMITS;
    }
    if(server->pending.cap < n) {
        if((e=minos_cp_reserve_exact(&server->pending, n)) < 0) {
            rwlock_end_write(&server->pending.lock);
            return e;
        }
    }
    rwlock_end_write(&server->pending.lock);
    sock->ops = &minos_server_ops;
    return 0;
}
static intptr_t minos_connect(Inode* sock, const struct sockaddr* addr, size_t addrlen) {
    (void)sock;
    (void)addr;
    (void)addrlen;
    MinOSClient* mc = &((MinOSSocket*)sock->priv)->client;
    if(addrlen != sizeof(struct sockaddr_minos) || addr->sa_family != AF_MINOS) {
        return -INVALID_PARAM;
    }
    if(!contains_null(((struct sockaddr_minos*)addr)->sminos_path, SOCKADDR_MINOS_PATH_MAX)) {
        return -INVALID_PATH;
    }
    Path path;
    intptr_t e = parse_abs(((struct sockaddr_minos*)addr)->sminos_path, &path);
    if(e < 0) return e;
    Inode* server_socket;
    if((e=vfs_find(&path, &server_socket)) < 0) return e;
    if(server_socket->kind != INODE_MINOS_SOCKET) {
        idrop(server_socket);
        return -INVALID_TYPE;
    }
    // FIXME: thread blocker for this
    while(server_socket->ops != &minos_server_ops) {
        if(server_socket->ops == &minos_client_ops) {
            idrop(server_socket);
            return -INVALID_TYPE;
        }
        asm volatile("hlt");
    }
    MinOSServer* server = &((MinOSSocket*)server_socket->priv)->server;
    rwlock_begin_write(&server->pending.lock);
    if(server->pending.len >= server->pending.cap) {
        rwlock_end_write(&server->pending.lock);
        idrop(server_socket);
        // TODO: better status message for this
        return -BUFFER_TOO_SMALL;
    }
    mc->pending = true;
    server->pending.items[server->pending.len++] = mc;
    rwlock_end_write(&server->pending.lock);
    // TODO: thread_blocker for this
    while(mc->pending && !mc->closed) asm volatile("hlt");
    sock->ops = &minos_client_ops;
    idrop(server_socket);
    return 0;
}
static InodeOps minos_socket_init_ops = { 
    .bind = minos_bind,
    .listen  = minos_listen,
    .connect = minos_connect, 
};
intptr_t minos_socket_init(Inode* sock) {
    if(!(sock->priv = minos_socket_new())) return -NOT_ENOUGH_MEM;
    sock->ops = &minos_socket_init_ops;
    return 0;
}
