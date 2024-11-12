#include "exception.h"
#include "kernel.h"
#include "debug.h"
#include "log.h"
typedef struct StackFrame {
     struct StackFrame* rbp;
     uintptr_t rip;
} StackFrame;

static void unwind_stack(IDTEFrame* frame) {
     //printf("+=== Stack Trace ===+\n");
     //printf("|%p   |\n",(void*)frame->rip);
     printf(" %p\n",(void*)frame->rip);
     StackFrame* stack = (StackFrame*)frame->rbp; 
     while(stack) {
        printf(" %p\n",(void*)stack->rip);
        //printf("|%p   |\n",(void*)stack->rip);
        stack = stack->rbp;
     }
     //printf("+======= End =======+\n");
}
const char* gpf_table[] = {
     "GDT",
     "IDT",
     "LDT",
     "IDT"
};
void exception_handler(IDTEFrame* frame) {
    if(frame->type == EXCEPTION_PAGE_FAULT) {
        kerror("Page fault at virtual address %p",(void*)frame->cr2);
    }
    if(frame->type == EXCEPTION_GPF) {
        kerror("General protection fault");
        if(frame->code) {
            printf("- External = %s\n", (frame->code >> 0) & 0b1 ? "true" : "false");
            printf("- Table    = %s\n", gpf_table[(frame->code >> 1) & 0b11]);
            printf("- Index    = 0x%02X\n", (uint16_t)frame->code >> 3);
        }
    }
    kinfo ("cr2=%p    type=%p    rip=%p    cs =%p    flags=%p    rsp=%p    ss =%p", (void*)frame->cr2, (void*)frame->type, (void*)frame->rip, (void*)frame->cs , (void*)frame->flags, (void*)frame->rsp, (void*)frame->ss );
    kinfo ("r15=%p    r14 =%p    r13=%p    r12=%p    r11  =%p    r10=%p    r9 =%p", (void*)frame->r15, (void*)frame->r14 , (void*)frame->r13, (void*)frame->r12, (void*)frame->r11  , (void*)frame->r10, (void*)frame->r9 );
    kinfo ("r8 =%p    rbp =%p    rdi=%p    rsi=%p    rdx  =%p    rcx=%p    rbx=%p", (void*)frame->r8 , (void*)frame->rbp , (void*)frame->rdi, (void*)frame->rsi, (void*)frame->rdx  , (void*)frame->rcx, (void*)frame->rbx);
    kinfo ("rax=%p\n"                                                               , (void*)frame->rax);
    kerror("Gotten exception (%zu) with code %zu at rip: %p at virtual: %p",frame->type, (size_t)frame->code,(void*)frame->rip,(void*)frame->cr2);
    if(!kernel.unwinding) {
        kernel.unwinding = true;
        unwind_stack(frame);
        kernel.unwinding = false;
    }
    kabort();
}

void init_exceptions() {
    idt_register(0 , idt_exception_division            , IDT_INTERRUPT_TYPE);
    idt_register(1 , idt_exception_debug               , IDT_INTERRUPT_TYPE);
    idt_register(3 , idt_exception_breakpoint          , IDT_INTERRUPT_TYPE);
    idt_register(4 , idt_exception_overflow            , IDT_INTERRUPT_TYPE);
    idt_register(5 , idt_exception_bound_range_exceeded, IDT_INTERRUPT_TYPE);
    idt_register(6 , idt_exception_invalid_opcode      , IDT_INTERRUPT_TYPE);

    idt_register(7 , idt_exception_dna                 , IDT_INTERRUPT_TYPE);
    idt_register(8 , idt_exception_double_fault        , IDT_INTERRUPT_TYPE);
    idt_register(9 , idt_exception_cop_segment_overrun , IDT_INTERRUPT_TYPE);
    idt_register(10, idt_exception_invalid_tss         , IDT_INTERRUPT_TYPE);
    idt_register(11, idt_exception_segment_not_present , IDT_INTERRUPT_TYPE);
    idt_register(12, idt_exception_ssf                 , IDT_INTERRUPT_TYPE);
    idt_register(13, idt_exception_gpf                 , IDT_INTERRUPT_TYPE);

    idt_register(14, idt_exception_page_fault          , IDT_INTERRUPT_TYPE);
    idt_register(16, idt_exception_floating_point      , IDT_INTERRUPT_TYPE);
    idt_register(17, idt_exception_alignment           , IDT_INTERRUPT_TYPE);
    idt_register(18, idt_exception_machine_check       , IDT_INTERRUPT_TYPE);
    idt_register(19, idt_exception_simd                , IDT_INTERRUPT_TYPE);
    idt_register(20, idt_exception_virtualization      , IDT_INTERRUPT_TYPE);
    idt_register(21, idt_exception_control_protection  , IDT_INTERRUPT_TYPE);
    idt_register(28, idt_exception_hypervisor_injection, IDT_INTERRUPT_TYPE);
    idt_register(29, idt_exception_vmm_comm            , IDT_INTERRUPT_TYPE);
    idt_register(30, idt_exception_security            , IDT_INTERRUPT_TYPE);
    idt_register(255, idt_spurious_interrupt           , IDT_INTERRUPT_TYPE);
}
