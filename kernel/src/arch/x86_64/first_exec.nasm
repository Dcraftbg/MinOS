[BITS 64]
section .text
global user_first_exec
user_first_exec:
    xor r15, r15
    xor r14, r14
    xor r13, r13
    xor r12, r12
    xor r11, r11
    xor r10, r10
    xor r9 , r9
    xor r8 , r8
    xor rbp, rbp
    ; xor rdi, rdi
    ; xor rsi, rsi
    ; xor rdx, rdx
    xor rbx, rbx
    xor rax, rax
    ; pop _start args
    pop rdi
    pop rsi
    pop rdx
    iretq
