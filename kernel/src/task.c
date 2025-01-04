#include "task.h"
#include "assert.h"
#include "kernel.h"
#include "mem/slab.h"
#include "string.h"
#include "port.h"
#include "exec.h"
#include "log.h"
#include "interrupt.h"

void pit_handler();
void init_tasks() {
    assert(kernel.task_cache = create_new_cache(sizeof(Task), "Task"));
    list_init(&kernel.tasks);
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
    if(kernel.interrupt_controller == &apic_controller) 
        irq_register(2, pit_handler, IRQ_FLAG_FAST);
    else 
        irq_register(0, pit_handler, IRQ_FLAG_FAST);
    pit_set_hz(1000);
}
Task* kernel_task_add() {
    Task* task = (Task*)cache_alloc(kernel.task_cache);
    if(task) {
        memset(task, 0, sizeof(Task));
        task->id = kernel.taskid++;
        list_init(&task->image.memlist);
        list_append(&task->list, &kernel.tasks);
    }
    return task;
}
void drop_task(Task* task) {
    if(task) {
        list_remove(&task->list);
        cache_dealloc(kernel.task_cache, task);
    }
}

Task* get_task_by_id(size_t id) {
    debug_assert(id != INVALID_TASK_ID);
    Task* task = (Task*)kernel.tasks.next;
    while(task != (Task*)&kernel.tasks) {
        if(task->id == id) return task;
        task = (Task*)task->list.next;
    }
    return NULL;
}
static Task* task_select(Task* ct) {
    Task* task = (Task*)(ct->list.next);
    while(task != ct) {
        if(task == (Task*)&kernel.tasks.prev || task->id == 0) goto skip;
        if((task->image.flags & TASK_FLAG_BLOCKING) && task->image.blocker.try_resolve(&task->image.blocker, task)) {
            task->image.flags &= ~TASK_FLAG_BLOCKING;
            return task;
        }
        if(task->image.flags & TASK_FLAG_PRESENT && (task->image.flags & TASK_FLAG_RUNNING) == 0)
            return task;
    skip:
        task = (Task*)task->list.next;
    }
    return NULL;
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
            irq_eoi(0);
            irq_ret_user(
                (uint64_t)select->image.ts_rsp,
                (uint64_t)select->image.cr3 & ~KERNEL_MEMORY_MASK,
                select->image.argc, select->image.argv,
                select->image.envc, select->image.envv
            );
        }
    }
    irq_eoi(0);
}
