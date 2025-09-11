#include <sys/socket.h>
#include <minos2errno.h>
#include "errno.h"

#include <minos/syscodes.h>
#include <minos/syscall.h>
#define syscall(sys, ...) \
    intptr_t e = sys(__VA_ARGS__); \
    return e < 0 ? (errno = _minos2errno(e), -1) : e

int socket(int domain, int type, int protocol) {
    syscall(syscall3, SYS_SOCKET, domain, type, protocol);
}
int connect(int fd, const struct sockaddr* addr, size_t addrlen) {
    syscall(syscall3, SYS_CONNECT, fd, addr, addrlen);
}
int bind(int fd, struct sockaddr* addr, size_t addrlen) {
    syscall(syscall3, SYS_BIND, fd, addr, addrlen);
}
int send(int fd, const void* buf, size_t n, int flags) {
    syscall(syscall3, SYS_SEND, fd, buf, n);
}
int recv(int fd, void *buf, size_t n, int flags) {
    syscall(syscall3, SYS_RECV, fd, buf, n);
}
int listen(int fd, int n) {
    syscall(syscall2, SYS_LISTEN, fd, n);
}
int accept(int fd, struct sockaddr* addr, size_t* addrlen) {
    syscall(syscall3, SYS_ACCEPT, fd, addr, addrlen);
}
