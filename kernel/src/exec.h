#pragma once
#include "process.h"
#include "task.h"
#include <minos/status.h>
#include "vfs.h"
#include "user.h"
#include "mem/memregion.h"
#include <stdint.h>
typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} IRQFrame;

intptr_t exec_new(const char* path, Args* args);
intptr_t exec(Task* task, const char* path, Args* args);
intptr_t fork_trampoline(Task* task, Task* result);
