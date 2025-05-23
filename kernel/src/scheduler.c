#include "scheduler.h"
#include "task.h"

static void scheduler_init(Scheduler* scheduler) {
    list_init(&scheduler->queue);
}
void init_schedulers(void) {
    for(size_t i = 0; i < kernel.max_processor_id+1; ++i) {
        scheduler_init(&kernel.processors[i].scheduler);
    }
}
Task* task_select(Scheduler* scheduler) {
    Task* task = NULL;
    mutex_lock(&scheduler->queue_mutex);
    if(list_empty(&scheduler->queue)) {
        mutex_unlock(&scheduler->queue_mutex);
        return NULL;
    }
    size_t n = list_len(&scheduler->queue);
    for(size_t i = 0; i < n; task=NULL, ++i) {
        task = (Task*)scheduler->queue.next;
        list_remove(&task->list);
        // Move it to the back
        list_insert(&task->list, &scheduler->queue);
        if(task->image.flags & TASK_FLAG_BLOCKING) {
            debug_assert(task->image.blocker.try_resolve);
            if(task->image.blocker.try_resolve(&task->image.blocker, task)) {
                task->image.flags &= ~TASK_FLAG_BLOCKING;
                break;
            }
            continue;
        }
        if((task->image.flags & TASK_FLAG_PRESENT) && (task->image.flags & TASK_FLAG_RUNNING) == 0) break;
    }
    mutex_unlock(&scheduler->queue_mutex);
    return task;
}
