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
static intptr_t minos_bind(Socket* sock, struct sockaddr* addr, size_t addrlen) {
    if(addrlen != sizeof(struct sockaddr_minos)) return -SIZE_MISMATCH;
    if(addr->sa_family != AF_MINOS) return -ADDR_SOCKET_FAMILY_MISMATCH;
    if(!contains_null(((MinOSSocket*)sock->priv)->addr, SOCKADDR_MINOS_PATH_MAX)) return -INVALID_PATH;
    // TODO: verify the path is null terminated
    memcpy(((MinOSSocket*)sock->priv)->addr, ((struct sockaddr_minos*)addr)->sminos_path, SOCKADDR_MINOS_PATH_MAX);
    return vfs_socket_create_abs(((MinOSSocket*)sock->priv)->addr, sock);
}
// Client Ops
static intptr_t minos_send(Socket* sock, const void* data, size_t size) {
    MinOSClient* mc = sock->priv;
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
static intptr_t minos_recv(Socket* sock, void* data, size_t size) {
    MinOSClient* mc = sock->priv;
    mutex_lock(&mc->data_lock);
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
// FIXME: close
static SocketOps minos_client_ops = {
    .recv = minos_recv,
    .send = minos_send,
};
// Server Ops
static intptr_t minos_accept(Socket* sock, Socket* result, struct sockaddr* addr, size_t *addrlen) {
    MinOSServer* server = sock->priv;
    rwlock_begin_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
    if(server->pools[MINOS_POOL_BACKLOGGED].len == 0) {
        rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
        return -WOULD_BLOCK;
    }
    MinOSClient* client = server->pools[MINOS_POOL_BACKLOGGED].items[0];
    result->priv = client;
    result->ops = &minos_client_ops;
    server->pools[MINOS_POOL_BACKLOGGED].len--;
    memmove(server->pools, server->pools + 1, server->pools[MINOS_POOL_BACKLOGGED].len * sizeof(server->pools[0]));
    rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
    return 0;
}
// FIXME: close
static SocketOps minos_server_ops = {
    .accept = minos_accept
};
// Unspecified operations.
static intptr_t minos_listen(Socket* sock, size_t n) {
    intptr_t e;
    MinOSServer* server = sock->priv;
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

    rwlock_begin_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
    if(server->pools[MINOS_POOL_BACKLOGGED].len > n) {
        kwarn("minos socket: called listen but backlog is bigger");
        rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
        return -LIMITS;
    }
    if(server->pools[MINOS_POOL_BACKLOGGED].cap < n) {
        if((e=minos_cp_reserve_exact(&server->pools[MINOS_POOL_BACKLOGGED], n)) < 0) {
            rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
            return e;
        }
    }
    rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
    sock->ops = &minos_server_ops;
    return 0;
}
static intptr_t minos_connect(Socket* sock, const struct sockaddr* addr, size_t addrlen) {
    (void)sock;
    (void)addr;
    (void)addrlen;
    MinOSClient* mc = sock->priv;
    if(addrlen != sizeof(struct sockaddr_minos) || addr->sa_family != AF_MINOS) return -INVALID_PARAM;
    if(!contains_null(((struct sockaddr_minos*)addr)->sminos_path, SOCKADDR_MINOS_PATH_MAX)) return -INVALID_PATH;
    Path path;
    intptr_t e = parse_abs(((struct sockaddr_minos*)addr)->sminos_path, &path);
    if(e < 0) return e;
    Inode* inode;
    if((e=vfs_find(&path, &inode)) < 0) return e;
    if(inode->superblock->fs != &tmpfs) {
        idrop(inode);
        return -INVALID_PATH;
    }
    if(inode->kind != INODE_MINOS_SOCKET) {
        idrop(inode);
        return -INVALID_TYPE;
    }
    Socket* server_socket = tmpfs_get_socket(inode);
    idrop(inode);
    assert(server_socket);
    assert(server_socket->family == AF_MINOS);
    if(server_socket->ops == &minos_client_ops) {
        kwarn("minos_connect tried to connect to client socket");
        return -INVALID_TYPE;
    }
    // FIXME: WOULD_BLOCK here and a thread blocker
    while(server_socket->ops != &minos_server_ops) {
        kwarn("server_socket->ops = %p", server_socket->ops);
        asm volatile("hlt");
    }
    MinOSServer* server = server_socket->priv;
    rwlock_begin_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
    if(server->pools[MINOS_POOL_BACKLOGGED].len >= server->pools[MINOS_POOL_BACKLOGGED].cap) {
        rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
        // TODO: better status message for this
        return -BUFFER_TOO_SMALL;
    }
    size_t idx = server->pools[MINOS_POOL_BACKLOGGED].len++;
    mc->server = server;
    mc->server_pool = MINOS_POOL_BACKLOGGED;
    mc->server_idx = idx;
    server->pools[MINOS_POOL_BACKLOGGED].items[idx] = mc;
    rwlock_end_write(&server->pools[MINOS_POOL_BACKLOGGED].lock);
    sock->ops = &minos_client_ops;
    return 0;
}
static intptr_t minos_close_unspec(Socket* sock) {
    cache_dealloc(minos_socket_cache, sock->priv);
    return 0;
}
static SocketOps minos_socket_init_ops = { 
    .bind = minos_bind,
    .listen  = minos_listen,
    .connect = minos_connect, 
    .close = minos_close_unspec,
};
intptr_t minos_socket_init(Socket* sock) {
    if(!(sock->priv = minos_socket_new())) return -NOT_ENOUGH_MEM;
    sock->family = AF_MINOS;
    sock->ops = &minos_socket_init_ops;
    return 0;
}
