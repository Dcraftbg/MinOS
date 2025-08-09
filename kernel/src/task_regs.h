#pragma once
struct task_regs {
    unsigned long rax;
    unsigned long rbx;
    unsigned long rcx;
    unsigned long rdx;
    unsigned long rsi;
    unsigned long rdi;
    unsigned long rdp;
    unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
    unsigned char fx[512];
};
