#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct ThreadBlocker ThreadBlocker;
typedef struct Task Task;
typedef struct Epoll Epoll;
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
        void* any;
    } as;
    bool (*try_resolve)(ThreadBlocker* blocker, Task* task);
};
bool try_resolve_waitpid(ThreadBlocker* blocker, Task* task);
bool try_resolve_sleep_until(ThreadBlocker* blocker, Task* task);
bool try_resolve_epoll(ThreadBlocker* blocker, Task* task);
int block_waitpid(Task* task, size_t child_index);
void block_sleepuntil(Task* task, size_t until);
void block_epoll(Task* task, Epoll* epoll, size_t until);
