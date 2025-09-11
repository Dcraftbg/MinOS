#include <sys/epoll.h>
#include <minos/sysstd.h>
#include <minos2errno.h>
#include <errno.h>

#include <minos/syscodes.h>
#include <minos/syscall.h>
#define syscall(sys, ...) \
    intptr_t e = sys(__VA_ARGS__); \
    return e < 0 ? (errno = _minos2errno(e), -1) : e

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    syscall(syscall4, SYS_EPOLL_CTL, epfd, op, fd, event);
}
int epoll_create1(int flags) {
    syscall(syscall1, SYS_EPOLL_CREATE1, flags);
}
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    syscall(syscall4, SYS_EPOLL_WAIT, epfd, events, maxevents, timeout);
}
