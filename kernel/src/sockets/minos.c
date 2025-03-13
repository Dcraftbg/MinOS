#include "minos.h"
#include <assert.h>
#include <mem/slab.h>
#include <minos/status.h>
#include "log.h"

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
static intptr_t minos_bind(Socket* sock, struct sockaddr* addr, size_t addrlen) {
    if(addrlen != sizeof(struct sockaddr_minos)) return -SIZE_MISMATCH;
    if(addr->sa_family != AF_MINOS) return -ADDR_SOCKET_FAMILY_MISMATCH;
    // TODO: verify the path is null terminated
    memcpy(((MinOSSocket*)sock->priv)->addr, ((struct sockaddr_minos*)addr)->sminos_path, SOCKADDR_MINOS_PATH_MAX);
    kwarn("TODO: actually register %s in the file system", ((struct sockaddr_minos*)addr)->sminos_path);
    // TODO: actually register it in the file system
    return 0;
}
static intptr_t minos_close_unspec(Socket* sock) {
    cache_dealloc(minos_socket_cache, sock->priv);
    return 0;
}
static SocketOps minos_server_ops = {
    NULL
};
static SocketOps minos_client_ops = {
    NULL
};
static intptr_t minos_listen(Socket* sock, size_t n) {
    (void)n;
    sock->ops = &minos_server_ops;
    kwarn("TBD minos_listen");
    return -UNSUPPORTED;
}
static intptr_t minos_connect(Socket* sock, const struct sockaddr* addr, size_t addrlen) {
    (void)sock;
    (void)addr;
    (void)addrlen;
    sock->ops = &minos_client_ops;
    kwarn("TBD minos_connect");
    return -UNSUPPORTED;
}
static SocketOps minos_socket_init_ops = { 
    .bind = minos_bind,
    .listen  = minos_listen,
    .connect = minos_connect, 
    .close = minos_close_unspec,
};
intptr_t minos_socket_init(Socket* sock) {
    if(!(sock->priv = minos_socket_new())) return -NOT_ENOUGH_MEM;
    sock->ops = &minos_socket_init_ops;
    return 0;
}
