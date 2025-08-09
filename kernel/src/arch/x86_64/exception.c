#include "exception.h"
#include "kernel.h"
#include "log.h"
#include "task.h"
#include "task_regs.h"
#include <mem/memregion.h>

typedef struct StackFrame {
    struct StackFrame* rbp;
    uintptr_t rip;
} StackFrame;
void unknown_handler() {
    kwarn("Unhandled interrupt");
}
static void unwind_stack(void* rip, void* rbp) {
    int depth = 10;
    ktrace(" %p", rip);
    StackFrame* stack = (StackFrame*)rbp; 
    while(stack && --depth) {
       ktrace(" %p",(void*)stack->rip);
       stack = stack->rbp;
    }
    if(depth == 0) ktrace(" [...]\n");
}
const char* gpf_table[] = {
    "GDT",
    "IDT",
    "LDT",
    "IDT"
};
static Mutex err = { 0 };
typedef struct {
    // GPRs (pushed by kernel)
    unsigned long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long rbp;
    unsigned long rdi;
    unsigned long rsi;
    unsigned long rdx;
    unsigned long rcx;
    unsigned long rbx;
    unsigned long rax;
    // interrupt vector that caused the interrupt
    unsigned long irq;
    unsigned long code;
    // amd64 interrupt frame
    unsigned long rip;
    unsigned long cs;
    unsigned long rflags;
    unsigned long rsp;
    unsigned long ss;
} ExceptionFrame;
enum {
    EXCEPTION_PAGE_FAULT = 14,
    EXCEPTION_GPF = 13,
};
void exception_handler(ExceptionFrame* frame, uint64_t cr2) {
    mutex_lock(&err);
    if(frame->irq == EXCEPTION_PAGE_FAULT) {
        kerror("Page fault at virtual address %p",(void*)cr2);
    }
    if(frame->irq == EXCEPTION_GPF) {
        kerror("General protection fault");
        if(frame->code) {
            kerror("- External = %s", (frame->code >> 0) & 0b1 ? "true" : "false");
            kerror("- Table    = %s", gpf_table[(frame->code >> 1) & 0b11]);
            kerror("- Index    = 0x%02X", (uint16_t)frame->code >> 3);
        }
    }
    kinfo ("cr2=%p    type=%p    rip=%p    cs =%p    flags=%p    rsp=%p    ss =%p", (void*)cr2, (void*)frame->irq, (void*)frame->rip, (void*)frame->cs , (void*)frame->rflags, (void*)frame->rsp, (void*)frame->ss );
    kinfo ("r15=%p    r14 =%p    r13=%p    r12=%p    r11  =%p    r10=%p    r9 =%p", (void*)frame->r15, (void*)frame->r14 , (void*)frame->r13, (void*)frame->r12, (void*)frame->r11  , (void*)frame->r10, (void*)frame->r9 );
    kinfo ("r8 =%p    rbp =%p    rdi=%p    rsi=%p    rdx  =%p    rcx=%p    rbx=%p", (void*)frame->r8 , (void*)frame->rbp , (void*)frame->rdi, (void*)frame->rsi, (void*)frame->rdx  , (void*)frame->rcx, (void*)frame->rbx);
    kinfo ("rax=%p\n"                                                               , (void*)frame->rax);
    kerror("Gotten exception (%zu) with code %zu at rip: %p at virtual: %p",frame->irq, (size_t)frame->code,(void*)frame->rip,(void*)cr2);
    Task* task  = current_task();

    // if(task) kinfo("the task at hand: %s", task->argv[0]);
#if 0
    if(task && frame->irq == EXCEPTION_PAGE_FAULT) {
#endif
        for(struct list* head = task->memlist.next; head != &task->memlist; head = head->next) {
            MemoryList* list = (MemoryList*)head;
            MemoryRegion* region = list->region;
            uintptr_t end = region->address + region->pages * PAGE_SIZE;
            kinfo(" %p -> %p (%zu pages)", region->address, end, region->pages);
            if(cr2 >= region->address && cr2 < end) {
                kwarn("Inside this region!");
            }
        }
#if 0
    }
#endif
    kerror("Fin...");
    mutex_unlock(&err);
#if 1
    if(!kernel.unwinding) {
        kernel.unwinding = true;
        unwind_stack((void*)frame->rip, (void*)frame->rbp);
        kernel.unwinding = false;
    }
#endif
    kabort();
}
// in assembly
extern void _exception_handler(TaskRegs*);
void init_exceptions(void) {
    for(size_t i = 0; i < 32; ++i) {
        irq_set_handler(i, _exception_handler);
    }
}
