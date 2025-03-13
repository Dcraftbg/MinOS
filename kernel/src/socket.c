#include "utils.h"
#include "socket.h"
#include "sockets/minos.h"
#include <minos/status.h>
static_assert(_AF_COUNT == 2, "Update socket_families");
SocketFamily socket_families[_AF_COUNT] = {
    [AF_UNSPEC] = { NULL },
    [AF_MINOS ] = { minos_socket_init }
};
// Wrappers
intptr_t socket_send(Socket* sock, const void* data, size_t size) {
    if(!sock->ops->send) return -UNSUPPORTED;
    return sock->ops->send(sock, data, size);
}
intptr_t socket_recv(Socket* sock,       void* data, size_t size) {
    if(!sock->ops->recv) return -UNSUPPORTED;
    return sock->ops->recv(sock, data, size);
}
intptr_t socket_accept(Socket* sock, Socket* result, struct sockaddr* addr, size_t *addrlen) {
    if(!sock->ops->accept) return -UNSUPPORTED;
    return sock->ops->accept(sock, result, addr, addrlen);
}
intptr_t socket_close(Socket* sock) {
    if(!sock->ops->close) return -UNSUPPORTED;
    return sock->ops->close(sock);
}
intptr_t socket_bind(Socket* sock, struct sockaddr* addr, size_t addrlen) {
    if(!sock->ops->bind) return -UNSUPPORTED;
    return sock->ops->bind(sock, addr, addrlen);
}
intptr_t socket_listen(Socket* sock, size_t n) {
    if(!sock->ops->listen) return -UNSUPPORTED;
    return sock->ops->listen(sock, n);
}
intptr_t socket_connect(Socket* sock, const struct sockaddr* addr, size_t addrlen) {
    if(!sock->ops->connect) return -UNSUPPORTED;
    return sock->ops->connect(sock, addr, addrlen);
}
// end Wrappers
