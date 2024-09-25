[BITS 64]
%include "arch/x86_64/asmstd.inc"
section .text
extern task_switch
global pit_handler

struc ContextFrame
   .cr3 resq 1
   .rsp resq 1
endstruc 
pit_handler:
   cld
   irq_push_regs
   mov rax, rsp
   sub rsp, ContextFrame_size
   mov [rsp+ContextFrame.rsp], rax
   mov rdi, rsp
   call task_switch
   mov rax, rsp
   mov rsp, [rax+ContextFrame.rsp]
   mov rax, [rax+ContextFrame.cr3]
   mov cr3, rax
   irq_pop_regs
   iretq
