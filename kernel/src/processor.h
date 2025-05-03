#pragma once
#include <collections/list.h>
#include <sync/mutex.h>
typedef struct Task Task;
// On a lapic system, we can have up to 256 logical processors
#define MAX_PROCESSORS 256 
typedef struct {
    size_t lapic_ticks;
    Mutex tasks_mutex;
    struct list tasks;
    Task* current_task;
} Processor;
