#pragma once
#include "task.h"
#include <status.h>
#include "vfs.h"
#include "user.h"
#include <stdint.h>
typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} IRQFrame;

intptr_t exec(const char* path, Args args);
