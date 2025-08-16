#pragma once
#include <collections/list.h>
#include "page.h"
#include "resource.h"
#include <stddef.h>

#define TASK_FLAG_PRESENT   0b1
#define TASK_FLAG_RUNNING   0b10
#define TASK_FLAG_FIRST_RUN 0b100
#define TASK_FLAG_USER      0b1000
// TODO: Remove this flag entirely.
// Replace with "dead" list
#define TASK_FLAG_DYING     0b10000
#define TASK_FLAG_BLOCKING  0b100000
#define INVALID_TASK_ID -1
#include "thread_blocker.h"

typedef struct Task Task;

struct Task {
    struct list list;
    size_t id;
    Process* process;
    uint64_t flags;
    page_t cr3;
    paddr_t cr3_phys;
    uintptr_t eoe;
    struct list memlist;
    // Task switch rsp
    // The rsp of the task switch at which the swap initialially happened
    // By default it starts 0xFFFFFFFFFFFFF000 as defined in the TSS
    void* ts_rsp;
    ThreadBlocker blocker;
    char name[4096];
};
void init_tasks();
void init_kernel_task();
Task* kernel_task_add();
Task* current_task();
void drop_task(Task* task);
