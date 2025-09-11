#pragma once
#include <list_head.h>
#include <sync/mutex.h>
typedef struct Task Task;
#include "scheduler.h"
#include <stdatomic.h>
// On a lapic system, we can have up to 256 logical processors
#define MAX_PROCESSORS 256 
typedef struct {
    size_t lapic_ticks;
    Scheduler scheduler;
    Task* current_task;
    atomic_bool initialised;
} Processor;
