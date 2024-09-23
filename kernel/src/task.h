#pragma once
#include "list.h"
#include "page.h"
#include "resource.h"
#include <stddef.h>
typedef struct {
    uint64_t flags;
    page_t cr3;
    size_t argc;
    const char** argv;
    struct list memlist;
    // Task switch rsp
    // The rsp of the task switch at which the swap initialially happened
    // By default it starts 0xFFFFFFFFFFFFF000 as defined in the TSS
    void* ts_rsp;
    uintptr_t rip;
} TaskImage;
static inline void taskimage_move(TaskImage* to, TaskImage* from) {
    memcpy(to, from, sizeof(*to));
    list_move(&to->memlist, &from->memlist);
    memset(from, 0, sizeof(*from));
    list_init(&from->memlist);
}
typedef struct {
    struct list list;
    size_t id;
    ResourceBlock* resources;
    TaskImage image;
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
Task* get_task_by_id(size_t id);
Task* kernel_task_add();
static Task* current_task() {
     return get_task_by_id(kernel.current_taskid);
}
void drop_task(Task* task);
