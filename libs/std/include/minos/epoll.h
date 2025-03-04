#pragma once
#include <stdint.h>
typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
#define EPOLLIN  (1 << 0)
#define EPOLLOUT (1 << 1)
struct epoll_event {
    uint32_t events;
    epoll_data_t data;
};
#define EPOLL_CTL_ADD 0
#define EPOLL_CTL_MOD 1
#define EPOLL_CTL_DEL 2
