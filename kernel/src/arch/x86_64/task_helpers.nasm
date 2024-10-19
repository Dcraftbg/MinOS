[BITS 64]
%include "asmstd.inc"
section .text
global irq_ret_user
; Accepts:
;    rdi = new task switch rsp
;    rsi = new task cr3
;    rdx = argc
;    rcx = argv
;    r8  = envc
;    r9  = envv
; Xored regisers:
;    rax, rbx
;    rbp
;    r8, r9, r10, r11, r12, r13, r14, r15
; Returns:
;    Never
irq_ret_user:
   mov rsp, rdi
   mov cr3, rsi
   mov rdi, rdx
   mov rsi, rcx
   mov rdx, r8
   mov rcx, r9
   xor rax, rax
   xor rbx, rbx
   xor rbp, rbp
   xor r8 , r8
   xor r9 , r9
   xor r10, r10
   xor r11, r11
   xor r12, r12
   xor r13, r13
   xor r14, r14
   xor r15, r15
   xchg bx, bx
   iretq
