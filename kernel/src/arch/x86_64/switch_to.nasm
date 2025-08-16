[BITS 64]
%include "task.inc"
section .text
; Implenented in C
extern _switch_to
global switch_to
; rdi - from (current task)
; rsi - to (outgoing task)
switch_to:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi + TASK_rsp], rsp
    mov rsp, [rsi + TASK_rsp]

    mov rax, [rsi + TASK_cr3]
    mov cr3, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    jmp _switch_to 
