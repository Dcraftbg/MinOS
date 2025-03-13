#include <sys/socket.h>
#include <minos/sysstd.h>
#include "stdio.h"
#include "errno.h"
int socket(int domain, int type, int protocol) {
    intptr_t e = _socket(domain, type, protocol);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
int connect(int fd, const struct sockaddr* addr, size_t addrlen) {
    intptr_t e = _connect(fd, addr, addrlen);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
int bind(int fd, struct sockaddr* addr, size_t addrlen) {
    intptr_t e = _bind(fd, addr, addrlen);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
int send(int fd, const void* buf, size_t n, int flags) {
    (void)flags;
    intptr_t e = _send(fd, buf, n);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
int recv(int fd, void *buf, size_t n, int flags) {
    (void)flags;
    intptr_t e = _recv(fd, buf, n);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
int listen(int fd, int n) {
    intptr_t e = _listen(fd, n);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
int accept(int fd, struct sockaddr* addr, size_t* addrlen) {
    intptr_t e = _accept(fd, addr, addrlen);
    if(e < 0) return (errno = _status_to_errno(e), -1);
    return e;
}
