[bits 64]
%include "asmstd.inc"
section .text
extern exception_handler
extern ps2_keyboard_handler
extern serial_handler
extern xhci_handler
extern unknown_handler

global idt_exception_division
global idt_exception_debug
global idt_exception_breakpoint
global idt_exception_overflow
global idt_exception_bound_range_exceeded
global idt_exception_invalid_opcode
global idt_exception_dna ; device not available
global idt_exception_double_fault
global idt_exception_cop_segment_overrun
global idt_exception_invalid_tss
global idt_exception_segment_not_present
global idt_exception_ssf ; stack segment fault
global idt_exception_gpf
global idt_exception_page_fault
global idt_exception_floating_point
global idt_exception_alignment
global idt_exception_machine_check
global idt_exception_simd
global idt_exception_virtualization
global idt_exception_control_protection
global idt_exception_hypervisor_injection
global idt_exception_vmm_comm
global idt_exception_security
global idt_spurious_interrupt


%macro register_irq 1
global idt_%1
align 0x08, db 0x00
idt_%1:
 irq_push_regs
 call %1
 irq_pop_regs
 iretq
%endmacro

register_irq ps2_keyboard_handler
register_irq serial_handler
register_irq unknown_handler
register_irq xhci_handler


idt_spurious_interrupt:
  iretq

; Exception handling

align 0x08, db 0x00
idt_exception_division:
   push 0
   push 0
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_debug:
   push 0
   push 1
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_breakpoint:
   push 0
   push 3
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_overflow:
   push 0
   push 4
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_bound_range_exceeded:
   push 0
   push 5
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_invalid_opcode:
   push 0
   push 6
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_dna:  ; Device not available
   push 0
   push 7
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_double_fault:
   push 8
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_cop_segment_overrun:
   push 0
   push 9
   jmp _idt_exception_base
align 0x08, db 0x00
idt_exception_invalid_tss:
   push 10
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_segment_not_present:
   push 11
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_ssf:   ; Stack segment fault
   push 12
   jmp _idt_exception_base
align 0x08, db 0x00
idt_exception_gpf:   ; General Protection fault
   push 13
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_page_fault:
   push 14
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_floating_point:
   push 0
   push 16
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_alignment:
   push 17
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_machine_check:
   push 0
   push 18
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_simd:
   push 0
   push 19
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_virtualization:
   push 0
   push 20
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_control_protection:
   push 21
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_hypervisor_injection:
   push 0
   push 28
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_vmm_comm:
   push 29
   jmp _idt_exception_base

align 0x08, db 0x00
idt_exception_security:
   push 30
   jmp _idt_exception_base
align 0x08, db 0x00
_idt_exception_base:
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
   mov rax, cr2
   push rax
   cld
   mov rdi, rsp
   call exception_handler 
   add rsp, 8
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
   add rsp, 0x10
   iretq
