#pragma once
#include "list.h"
#include "page.h"
#include "resource.h"
#include <stddef.h>
typedef struct {
    struct list list;
    size_t id;
    uint64_t flags;
    page_t cr3;

    ResourceBlock* resources;
    // Task switch rsp
    // The rsp of the task switch at which the swap initialially happened
    // By default it starts 0xFFFFFFFFFFFFF000 as defined in the TSS
    void* ts_rsp;
    void* rip;
} Task;
#define TASK_FLAG_PRESENT   0b1
#define TASK_FLAG_RUNNING   0b10
#define TASK_FLAG_FIRST_RUN 0b100
#define TASK_FLAG_USER      0b1000
#define TASK_FLAG_DYING     0b10000
#define INVALID_TASK_ID -1
void init_tasks();
void init_kernel_task();
void init_task_switch();
Task* kernel_task_add();
Task* current_task();
void drop_task(Task* task);