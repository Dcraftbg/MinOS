#pragma once
#include "list.h"
#include "page.h"
#include "resource.h"
#include <stddef.h>

typedef struct {
    size_t bytelen; // Calculated using argv[0..argc].sum(arg.len());
    size_t argc;
    const char** argv;
} Args;
static Args create_args(size_t argc, const char** argv) {
    size_t sum = 0;
    for(size_t i = 0; i < argc; ++i) {
        sum += strlen(argv[i])+1;
    }
    return (Args) { sum, argc, argv };
}
typedef struct {
    struct list list;
    size_t id;
    uint64_t flags;
    page_t cr3;
    size_t argc;
    const char** argv;

    ResourceBlock* resources;
    // Task switch rsp
    // The rsp of the task switch at which the swap initialially happened
    // By default it starts 0xFFFFFFFFFFFFF000 as defined in the TSS
    void* ts_rsp;
    uintptr_t rip;
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
