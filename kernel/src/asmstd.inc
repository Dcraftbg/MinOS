; -- General macros for EOU for irqs --
PTR_SIZE equ 8
IRQ_REGS_SIZE equ PTR_SIZE*15
%macro irq_push_regs 0
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
%endmacro

%macro xor_regs 0
 xor r15, r15
 xor r14, r14
 xor r13, r13
 xor r12, r12
 xor r11, r11
 xor r10, r10
 xor r9 , r9
 xor r8 , r8
 xor rbp, rbp
 xor rdi, rdi
 xor rsi, rsi
 xor rdx, rdx
 xor rcx, rcx
 xor rbx, rbx
 xor rax, rax
%endmacro
%macro irq_pop_regs 0
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
%endmacro


