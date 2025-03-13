#pragma once
#include <minos/socket.h>
#include <stddef.h>
#include <stdint.h>
int socket(int domain, int type, int protocol);
int connect(int fd, const struct sockaddr* addr, size_t addrlen); 
int bind(int fd, struct sockaddr* addr, size_t addrlen);
int send(int fd, const void* buf, size_t n, int flags);
int recv(int fd, void *buf, size_t n, int flags);
int listen(int fd, int n);
int accept(int fd, struct sockaddr* addr, size_t* addrlen);
