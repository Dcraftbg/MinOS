#pragma once
typedef struct TaskRegs {
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
    // amd64 interrupt frame
    unsigned long rip;
    unsigned long cs;
    unsigned long rflags;
    unsigned long rsp;
    unsigned long ss;
} TaskRegs;

