#pragma once
#include <minos/socket.h>
#include <stddef.h>
#include <stdint.h>
typedef struct Socket Socket;
typedef struct SocketOps SocketOps;
struct SocketOps {
    intptr_t (*send)(Socket* sock, const void* data, size_t size);
    intptr_t (*recv)(Socket* sock,       void* data, size_t size);
    intptr_t (*accept)(Socket* sock, Socket* result, struct sockaddr* addr, size_t *addrlen);
    intptr_t (*close)(Socket* sock);
    intptr_t (*bind)(Socket* sock, struct sockaddr* addr, size_t addrlen);
    intptr_t (*listen)(Socket* sock, size_t n);
    intptr_t (*connect)(Socket* sock, const struct sockaddr* addr, size_t addrlen);
};
// TODO: Think about: maybe its a good idea to have a connection pool and backlog here
// as its the same for most sockets but then again I'm not entirely sure
struct Socket {
    sa_family_t family;
    void* priv;
    SocketOps* ops;
};
intptr_t socket_send(Socket* sock, const void* data, size_t size);
intptr_t socket_recv(Socket* sock,       void* data, size_t size);
intptr_t socket_accept(Socket* sock, Socket* result, struct sockaddr* addr, size_t *addrlen);
intptr_t socket_close(Socket* sock);
intptr_t socket_bind(Socket* sock, struct sockaddr* addr, size_t addrlen);
intptr_t socket_listen(Socket* sock, size_t n);
intptr_t socket_connect(Socket* sock, const struct sockaddr* addr, size_t addrlen);
typedef struct {
    intptr_t (*init)(Socket* sock);
} SocketFamily;

extern SocketFamily socket_families[_AF_COUNT];
