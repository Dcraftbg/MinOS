#pragma once
#include <minos/epoll.h>
#include <stddef.h>
#include <stdint.h>
#include <collections/list.h>

typedef struct Epoll Epoll;
typedef struct Process Process;
typedef struct {
    struct list list;
    int fd;
    uint32_t result_events;
    struct epoll_event event;
} EpollFd;
#include "vfs.h"
struct Epoll {
    Inode inode;
    struct list unready;
    struct list ready;
};
Epoll* epoll_new(void);
intptr_t epoll_add(Epoll* epoll, EpollFd* fd);
intptr_t epoll_del(Epoll* epoll, int fd);
intptr_t epoll_mod(Epoll* epoll, int fd, const struct epoll_event* event);
EpollFd* epoll_fd_new(int fd, const struct epoll_event* event);
void epoll_fd_delete(EpollFd* fd);
bool epoll_poll(Epoll* epoll, Process* process);
void init_epoll_cache(void);
