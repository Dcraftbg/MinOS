[BITS 64]
%include "asmstd.inc"
extern msi_dispatch
msi_base:
   push rax
   push rbx
   push rcx
   push rdx
   push rsi
   push rdi
   push rbp
   push r8
   push r9
   push r10
   push r11
   push r12
   push r13
   push r14
   push r15
   mov rdi, rsp
   call msi_dispatch
   pop r15
   pop r14
   pop r13
   pop r12
   pop r11
   pop r10
   pop r9
   pop r8
   pop rbp
   pop rdi
   pop rsi
   pop rdx
   pop rcx
   pop rbx
   pop rax
   add rsp, 0x8
   iretq
; Automatically generated by msi_dispatcher.c (Not included in repo)
global msi_irq_0
msi_irq_0:
   push 0
   jmp msi_base
global msi_irq_1
msi_irq_1:
   push 1
   jmp msi_base
global msi_irq_2
msi_irq_2:
   push 2
   jmp msi_base
global msi_irq_3
msi_irq_3:
   push 3
   jmp msi_base
global msi_irq_4
msi_irq_4:
   push 4
   jmp msi_base
global msi_irq_5
msi_irq_5:
   push 5
   jmp msi_base
global msi_irq_6
msi_irq_6:
   push 6
   jmp msi_base
global msi_irq_7
msi_irq_7:
   push 7
   jmp msi_base
global msi_irq_8
msi_irq_8:
   push 8
   jmp msi_base
global msi_irq_9
msi_irq_9:
   push 9
   jmp msi_base
global msi_irq_10
msi_irq_10:
   push 10
   jmp msi_base
global msi_irq_11
msi_irq_11:
   push 11
   jmp msi_base
global msi_irq_12
msi_irq_12:
   push 12
   jmp msi_base
global msi_irq_13
msi_irq_13:
   push 13
   jmp msi_base
global msi_irq_14
msi_irq_14:
   push 14
   jmp msi_base
global msi_irq_15
msi_irq_15:
   push 15
   jmp msi_base
global msi_irq_16
msi_irq_16:
   push 16
   jmp msi_base
global msi_irq_17
msi_irq_17:
   push 17
   jmp msi_base
global msi_irq_18
msi_irq_18:
   push 18
   jmp msi_base
global msi_irq_19
msi_irq_19:
   push 19
   jmp msi_base
global msi_irq_20
msi_irq_20:
   push 20
   jmp msi_base
global msi_irq_21
msi_irq_21:
   push 21
   jmp msi_base
global msi_irq_22
msi_irq_22:
   push 22
   jmp msi_base
global msi_irq_23
msi_irq_23:
   push 23
   jmp msi_base
global msi_irq_24
msi_irq_24:
   push 24
   jmp msi_base
global msi_irq_25
msi_irq_25:
   push 25
   jmp msi_base
global msi_irq_26
msi_irq_26:
   push 26
   jmp msi_base
global msi_irq_27
msi_irq_27:
   push 27
   jmp msi_base
global msi_irq_28
msi_irq_28:
   push 28
   jmp msi_base
global msi_irq_29
msi_irq_29:
   push 29
   jmp msi_base
global msi_irq_30
msi_irq_30:
   push 30
   jmp msi_base
global msi_irq_31
msi_irq_31:
   push 31
   jmp msi_base
global msi_irq_32
msi_irq_32:
   push 32
   jmp msi_base
global msi_irq_33
msi_irq_33:
   push 33
   jmp msi_base
global msi_irq_34
msi_irq_34:
   push 34
   jmp msi_base
global msi_irq_35
msi_irq_35:
   push 35
   jmp msi_base
global msi_irq_36
msi_irq_36:
   push 36
   jmp msi_base
global msi_irq_37
msi_irq_37:
   push 37
   jmp msi_base
global msi_irq_38
msi_irq_38:
   push 38
   jmp msi_base
global msi_irq_39
msi_irq_39:
   push 39
   jmp msi_base
global msi_irq_40
msi_irq_40:
   push 40
   jmp msi_base
global msi_irq_41
msi_irq_41:
   push 41
   jmp msi_base
global msi_irq_42
msi_irq_42:
   push 42
   jmp msi_base
global msi_irq_43
msi_irq_43:
   push 43
   jmp msi_base
global msi_irq_44
msi_irq_44:
   push 44
   jmp msi_base
global msi_irq_45
msi_irq_45:
   push 45
   jmp msi_base
global msi_irq_46
msi_irq_46:
   push 46
   jmp msi_base
global msi_irq_47
msi_irq_47:
   push 47
   jmp msi_base
