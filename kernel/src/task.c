#include "task.h"
#include "assert.h"
#include "kernel.h"
#include "slab.h"
#include "string.h"
#include "port.h"
#include "pic.h"
#include "exec.h"
void pit_handler();
void init_tasks() {
    assert(kernel.task_cache = create_new_cache(sizeof(Task), "Task"));
    list_init(&kernel.tasks);
}

void init_kernel_task() {
    Task* kt = NULL;
    assert(kt = kernel_task_add());
    kt->cr3 = kernel.pml4;
    kt->flags |= TASK_FLAG_PRESENT | TASK_FLAG_RUNNING;
    kernel.current_taskid = kt->id;

    IRQFrame* frame = (IRQFrame*)(virt_to_phys(kt->cr3, KERNEL_STACK_PTR) | KERNEL_MEMORY_MASK);
    frame->cs     = GDT_KERNELCODE; 
    frame->rflags = 0x286;
    frame->ss     = GDT_KERNELDATA;
    frame->rsp    = KERNEL_STACK_PTR;
    kt->ts_rsp = (void*)(KERNEL_STACK_PTR+sizeof(IRQFrame));
    kt->rip = 0;
}

void init_task_switch() {
    idt_register(0x20, pit_handler, IDT_HARDWARE_TYPE);
}
Task* kernel_task_add() {
    Task* task = (Task*)cache_alloc(kernel.task_cache);
    if(task) {
        memset(task, 0, sizeof(Task));
        task->id = kernel.taskid++;
        list_append(&task->list, &kernel.tasks);
    }
    return task;
}
void drop_task(Task* task) {
    if(task) {
        cache_dealloc(kernel.task_cache, task);
    }
}
Task* current_task() {
    debug_assert(kernel.current_taskid != INVALID_TASK_ID);
    Task* task = (Task*)kernel.tasks.next;
    while(task != (Task*)&kernel.tasks) {
        if(task->id == kernel.current_taskid) return task;
        task = (Task*)task->list.next;
    }
    return NULL;
}
static Task* task_select(Task* ct) {
    Task* task = (Task*)(ct->list.next);
    while(task != ct) {
        if(task == (Task*)&kernel.tasks.prev || task->id == 0) {
           task = (Task*)task->list.next;
           continue;
        }
        if(task->flags & TASK_FLAG_PRESENT && (task->flags & TASK_FLAG_RUNNING) == 0) {
           return task;
        }
        task = (Task*)task->list.next;
    }
    return NULL;
}
__attribute__((optimize("O3")))
void task_switch() {
    Task* current = NULL;
    current = current_task();
    debug_assert(current);
    asm volatile (
        "movq %%rsp, %0"
        : "=r" (current->ts_rsp)
    );
    Task* select = NULL;
    if((select = task_select(current))) {
        current->flags &= ~TASK_FLAG_RUNNING;
        // printf("SELECT: %p\n",select);
        __asm__ volatile (
            "movq %0, %%cr3\n"
            :: "r" ((uintptr_t)select->cr3 & ~KERNEL_MEMORY_MASK)
        );
        __asm__ volatile (
            "movq %0, %%rsp"
            :
            : "r" (select->ts_rsp)
        );
        //printf("Got here!\n");
        //printf("current: %p\n",current);
        select->flags |= TASK_FLAG_RUNNING;
        kernel.current_taskid = select->id;
        if (select->flags & TASK_FLAG_FIRST_RUN) {
            // printf("Got here!\n");
            select->flags &= ~TASK_FLAG_FIRST_RUN;
            outb(PIC1_CMD, 0x20);
            asm volatile (
               ""
               :
               : "D"(select->argc), "S"(select->argv)
            );
            asm volatile (
               "xor %rax, %rax\n"
               "xor %rbx, %rbx\n"
               "xor %rcx, %rcx\n"
               "xor %rdx, %rdx\n"
               // "xor %rsi, %rsi\n"
               // "xor %rdi, %rdi\n"
               "xor %rbp, %rbp\n"
               "xor %r8 , %r8 \n"
               "xor %r9 , %r9 \n"
               "xor %r10, %r10\n"
               "xor %r11, %r11\n"
               "xor %r12, %r12\n"
               "xor %r13, %r13\n"
               "xor %r14, %r14\n"
               "xor %r15, %r15\n"
               "xchg %bx, %bx\n"
               "iretq"
            );
        }
    }
    pic_end(0);
}
