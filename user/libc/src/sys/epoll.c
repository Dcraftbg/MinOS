#include "sys/epoll.h"
#include "stdio.h"
#include <minos/sysstd.h>
#include "errno.h"
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    intptr_t e = _epoll_ctl(epfd, op, fd, event);
    if(e < 0) return -(errno = _status_to_errno(e));
    return e;
}
int epoll_create1(int flags) {
    intptr_t e = _epoll_create1(flags);
    if(e < 0) return -(errno = _status_to_errno(e));
    return e;
}
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    intptr_t e = _epoll_wait(epfd, events, maxevents, timeout);
    if(e < 0) {
        errno = _status_to_errno(e);
        return -1;
    }
    return e;
}
