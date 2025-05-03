#pragma once
#include <collections/list.h>
#include <sync/mutex.h>
typedef struct Task Task;
typedef struct {
    // TODO: async queue (MPSC) :)
    struct list queue;
    Mutex queue_mutex;
} Scheduler;
void init_schedulers(void);
Task* task_select(Scheduler* scheduler);
