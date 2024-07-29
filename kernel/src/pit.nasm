[BITS 64]
%include "asmstd.inc"
section .text
extern task_switch
global pit_handler

pit_handler:
   cld
   irq_push_regs
   call task_switch
   irq_pop_regs
   iretq

