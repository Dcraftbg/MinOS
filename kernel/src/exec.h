#pragma once
#include "process.h"
#include "task.h"
#include <minos/status.h>
#include "vfs.h"
#include "user.h"
#include "mem/memregion.h"
#include <stdint.h>

intptr_t exec_new(const char* path, Args* args, Args* env);
intptr_t exec(Task* task, Path* path, Args* args, Args* envs);
intptr_t fork_trampoline(Task* task, Task* result);
