[BITS 64]
%include "arch/x86_64/asmstd.inc"
section .text
extern task_switch
global task_switch_handler

struc ContextFrame
   .cr3 resq 1
   .rsp resq 1
endstruc 
extern get_lapic_id
task_switch_handler:
   irq_push_regs
   call get_lapic_id
   cmp rax, 0
   jne .end
   mov rax, rsp
   sub rsp, ContextFrame_size
   mov [rsp+ContextFrame.rsp], rax
   mov rdi, rsp
   call task_switch
   mov rax, rsp
   mov rsp, [rax+ContextFrame.rsp]
   mov rax, [rax+ContextFrame.cr3]
   mov cr3, rax
.end:
   irq_pop_regs
   iretq
