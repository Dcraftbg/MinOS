#pragma once
#include <list_head.h>
#include "page.h"
#include "resource.h"
#include <stddef.h>

#define TASK_FLAG_BLOCKING  0b1

#define INVALID_TASK_ID -1
#include "thread_blocker.h"

typedef struct Process Process;
typedef struct Task {
    struct list_head list;
    size_t id;
    Process* process;
    uint64_t flags;
    page_t cr3;
    paddr_t cr3_phys;
    uintptr_t eoe;
    struct list_head memlist;
    // Task switch rsp
    // The rsp of the task switch at which the swap initialially happened
    // By default it starts 0xFFFFFFFFFFFFF000 as defined in the TSS
    void* rsp;
    ThreadBlocker blocker;
    char name[4096];
} Task;
void init_tasks();
void init_kernel_task();
Task* kernel_task_add();
Task* current_task();
void drop_task(Task* task);
