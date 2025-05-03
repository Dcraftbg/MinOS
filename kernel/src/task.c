#include "task.h"
#include "assert.h"
#include "kernel.h"
#include "mem/slab.h"
#include "string.h"
#include "port.h"
#include "exec.h"
#include "log.h"
#include "interrupt.h"

void task_switch_handler();
void init_tasks() {
    assert(kernel.task_cache = create_new_cache(sizeof(Task), "Task"));
}

void init_kernel_task() {
    Task* kt = NULL;
    assert(kt = kernel_task_add());
    kt->image.cr3 = kernel.pml4;
    kt->image.flags |= TASK_FLAG_RUNNING;
    kernel.current_taskid = kt->id;
    kt->image.ts_rsp = 0;
    kt->image.rip = 0;
}
#include "apic.h"
void init_task_switch() {
    irq_register(kernel.task_switch_irq, task_switch_handler, IRQ_FLAG_FAST);
}
Task* kernel_task_add() {
    mutex_lock(&kernel.tasks_mutex);
    if(!ptr_darray_reserve(&kernel.tasks, 1)) {
        mutex_unlock(&kernel.tasks_mutex);
        return NULL;
    }
    size_t id = ptr_darray_pick_empty_slot(&kernel.tasks);
    Task* task = (Task*)cache_alloc(kernel.task_cache);
    if(!task) {
        mutex_unlock(&kernel.tasks_mutex);
        return NULL;
    }
    if(id == kernel.tasks.len) kernel.tasks.len++;
    memset(task, 0, sizeof(Task));
    task->id = id;
    list_init(&task->image.memlist);
    kernel.tasks.items[id] = task;
    mutex_unlock(&kernel.tasks_mutex);
    return task;
}
void drop_task(Task* task) {
    if(task) {
        list_remove(&task->list);
        cache_dealloc(kernel.task_cache, task);
    }
}

Task* get_task_by_id(size_t id) {
    if(id == INVALID_TASK_ID) return NULL;
    mutex_lock(&kernel.tasks_mutex);
    if(id >= kernel.tasks.len) {
        mutex_unlock(&kernel.tasks_mutex);
        return NULL;
    }
    Task* task = kernel.tasks.items[id];
    mutex_unlock(&kernel.tasks_mutex);
    return task;
}
static Task* task_select(Task* ct) {
#if 1
    debug_assert(kernel.tasks.len);
    size_t id = ct->id; 
    Task* task;
    for(;;) {
        id = (id + 1) % kernel.tasks.len;
        task = kernel.tasks.items[id];
        if(!task) continue;
        if(task->image.flags & TASK_FLAG_BLOCKING) {
            // I have been burnt by this shit already. I don't care if the assertion
            // is slightly slower
            debug_assert(task->image.blocker.try_resolve);
            if(task->image.blocker.try_resolve(&task->image.blocker, task)) {
                task->image.flags &= ~TASK_FLAG_BLOCKING;
                break;
            }
            continue;
        }
        if((task->image.flags & TASK_FLAG_PRESENT) && (task->image.flags & TASK_FLAG_RUNNING) == 0) break;
    }
    return task;
#else
    Task* task = (Task*)(ct->list.next);
    while(task != ct) {
        if(task == (Task*)&kernel.tasks.prev || task->id == 0) goto skip;
        if((task->image.flags & TASK_FLAG_BLOCKING)) {
            // I have been burnt by this shit already. I don't care if the assertion
            // is slightly slower
            debug_assert(task->image.blocker.try_resolve);
            if(task->image.blocker.try_resolve(&task->image.blocker, task)) {
                task->image.flags &= ~TASK_FLAG_BLOCKING;
                return task;
            }
        }
        if(task->image.flags & TASK_FLAG_PRESENT && (task->image.flags & TASK_FLAG_RUNNING) == 0)
            return task;
    skip:
        task = (Task*)task->list.next;
    }
    return NULL;
#endif
}


typedef struct {
    uint64_t cr3;
    uint64_t rsp;
} ContextFrame;

__attribute__((noreturn)) 
void irq_ret_user(uint64_t ts_rsp, uint64_t cr3, uint64_t argc, const char** argv, uint64_t envc, const char** envv);


__attribute__((optimize("O3")))
void task_switch(ContextFrame* frame) {
    Task* current = current_task();
    debug_assert(current);
    frame->cr3 = (uintptr_t)current->image.cr3 & ~KERNEL_MEMORY_MASK;
    current->image.ts_rsp = (void*)frame->rsp;
    Task* select = task_select(current);
    kernel.pit_info.ticks++;
    if(select) {
        current->image.flags &= ~TASK_FLAG_RUNNING;
        frame->cr3 = (uintptr_t)select->image.cr3 & ~KERNEL_MEMORY_MASK;
        frame->rsp = (uintptr_t)select->image.ts_rsp;
        select->image.flags |= TASK_FLAG_RUNNING;
        kernel.current_taskid = select->id;
        kernel.current_processid = select->processid;
        if (select->image.flags & TASK_FLAG_FIRST_RUN) {
            select->image.flags &= ~TASK_FLAG_FIRST_RUN;
            irq_eoi(kernel.task_switch_irq);
            irq_ret_user(
                (uint64_t)select->image.ts_rsp,
                (uint64_t)select->image.cr3 & ~KERNEL_MEMORY_MASK,
                select->image.argc, select->image.argv,
                select->image.envc, select->image.envv
            );
        }
    }
    irq_eoi(kernel.task_switch_irq);
}
