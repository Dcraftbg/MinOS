#include "task.h"
#include "assert.h"
#include "kernel.h"
#include "mem/slab.h"
#include "string.h"
#include "port.h"
#include "exec.h"
#include "log.h"
#include "interrupt.h"
#include "apic.h"
#include "arch/x86_64/idt.h"

void task_switch_handler();
void init_tasks() {
    assert(kernel.task_cache = create_new_cache(sizeof(Task), "Task"));
}

void init_kernel_task() {
    Task* kt = NULL;
    assert(kt = kernel_task_add());
    kt->cr3 = kernel.pml4;
    kt->flags |= TASK_FLAG_RUNNING;
    for(size_t i = 0; i <= kernel.max_processor_id; ++i) {
        kernel.processors[i].current_task = kt;
    }
    kt->ts_rsp = 0;
    kt->rip = 0;
}
void init_task_switch() {
    idt_register(kernel.task_switch_irq, task_switch_handler, IDT_INTERRUPT_TYPE);
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
    list_init(&task->list);
    list_init(&task->memlist);
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

Task* current_task(void) {
    return kernel.processors[get_lapic_id()].current_task;
}
/*
static Task* task_select(Task* ct) {
#if 1
    debug_assert(kernel.tasks.len);
    size_t id = ct->id; 
    Task* task;
    for(;;) {
        id = (id + 1) % kernel.tasks.len;
        task = kernel.tasks.items[id];
        if(!task) continue;
        if(task->flags & TASK_FLAG_BLOCKING) {
            // I have been burnt by this shit already. I don't care if the assertion
            // is slightly slower
            debug_assert(task->blocker.try_resolve);
            if(task->blocker.try_resolve(&task->blocker, task)) {
                task->flags &= ~TASK_FLAG_BLOCKING;
                break;
            }
            continue;
        }
        if((task->flags & TASK_FLAG_PRESENT) && (task->flags & TASK_FLAG_RUNNING) == 0) break;
    }
    return task;
#else
    Task* task = (Task*)(ct->list.next);
    while(task != ct) {
        if(task == (Task*)&kernel.tasks.prev || task->id == 0) goto skip;
        if((task->flags & TASK_FLAG_BLOCKING)) {
            // I have been burnt by this shit already. I don't care if the assertion
            // is slightly slower
            debug_assert(task->blocker.try_resolve);
            if(task->blocker.try_resolve(&task->blocker, task)) {
                task->flags &= ~TASK_FLAG_BLOCKING;
                return task;
            }
        }
        if(task->flags & TASK_FLAG_PRESENT && (task->flags & TASK_FLAG_RUNNING) == 0)
            return task;
    skip:
        task = (Task*)task->list.next;
    }
    return NULL;
#endif
}
*/

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
    frame->cr3 = (uintptr_t)current->cr3 & ~KERNEL_MEMORY_MASK;
    current->ts_rsp = (void*)frame->rsp;
    Processor* processor = &kernel.processors[get_lapic_id()];
    Task* select = task_select(&processor->scheduler);
    processor->lapic_ticks++;
    if(select) {
        current->flags &= ~TASK_FLAG_RUNNING;
        frame->cr3 = (uintptr_t)select->cr3 & ~KERNEL_MEMORY_MASK;
        frame->rsp = (uintptr_t)select->ts_rsp;
        select->flags |= TASK_FLAG_RUNNING;
        processor->current_task = select;
        if (select->flags & TASK_FLAG_FIRST_RUN) {
            select->flags &= ~TASK_FLAG_FIRST_RUN;
            irq_eoi(kernel.task_switch_irq);
            irq_ret_user(
                (uint64_t)select->ts_rsp,
                (uint64_t)select->cr3 & ~KERNEL_MEMORY_MASK,
                select->argc, select->argv,
                select->envc, select->envv
            );
        }
    }
    irq_eoi(kernel.task_switch_irq);
}
