#pragma once
#include <minos/epoll.h>
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_create1(int flags);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
