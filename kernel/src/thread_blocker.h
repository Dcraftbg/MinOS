#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct ThreadBlocker ThreadBlocker;
typedef struct Task Task;
struct ThreadBlocker {
    union {
        struct {
            uint16_t child_index;
            uint32_t exit_code;
        } waitpid;
        void* any;
    } as;
    bool (*try_resolve)(ThreadBlocker* blocker, Task* task);
};
bool try_resolve_waitpid(ThreadBlocker* blocker, Task* task);
int block_waitpid(Task* task, size_t child_index);
