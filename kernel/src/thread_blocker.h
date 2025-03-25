#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct ThreadBlocker ThreadBlocker;
typedef struct Task Task;
typedef struct Epoll Epoll;
typedef struct Inode Inode;
#include <minos/socket.h>
struct ThreadBlocker {
    union {
        struct {
            uint16_t child_index;
            uint32_t exit_code;
        } waitpid;
        struct {
            size_t until;
        } sleep;
        struct {
            // NOTE: Contains pointer to epoll which means if another thread
            // closes it it will be a hanging pointer. Fix this
            Epoll* epoll;
            size_t until;
        } epoll;
        struct {
            // NOTE: Contains res to epoll which means if another thread
            // closes it it will be a hanging pointer. Fix this
            Inode* sock;
            Inode* result;
            intptr_t e;
            // We copy both parameters in for CPU cache optimisations reasons
            // So we don't need to reload the cr3 which is kind of expensive and flushes the cache
            size_t addrlen;
            char addr_buf[_sockaddr_max];
        } accept;
        void* any;
    } as;
    bool (*try_resolve)(ThreadBlocker* blocker, Task* task);
};
bool try_resolve_waitpid(ThreadBlocker* blocker, Task* task);
bool try_resolve_sleep_until(ThreadBlocker* blocker, Task* task);
bool try_resolve_epoll(ThreadBlocker* blocker, Task* task);
bool try_resolve_accept(ThreadBlocker* blocker, Task* task);
int block_waitpid(Task* task, size_t child_index);
void block_sleepuntil(Task* task, size_t until);
void block_epoll(Task* task, Epoll* epoll, size_t until);
intptr_t block_accept(Task* task, Inode* sock, Inode* result, struct sockaddr* addr, size_t* addrlen);
