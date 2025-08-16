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

void init_tasks() {
    assert(kernel.task_cache = create_new_cache(sizeof(Task), "Task"));
}

void init_kernel_task() {
    Task* kt = NULL;
    assert(kt = kernel_task_add());
    memcpy(kt->name, "<kernel>", 9);
    kt->cr3 = kernel.pml4;
    for(size_t i = 0; i <= kernel.max_processor_id; ++i) {
        kernel.processors[i].current_task = kt;
    }
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
