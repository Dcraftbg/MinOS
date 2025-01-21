[BITS 64]

section .text

global setjmp
global longjmp

struc jmp_buf
    .rip resq 1
    .rsp resq 1
    .rbp resq 1
    .r12 resq 1
    .r13 resq 1
    .r14 resq 1
    .r15 resq 1
endstruc
; rdi Pointer to jmp_buf 
setjmp:
    mov rax, [rsp]
    mov [rdi+jmp_buf.rip], rax
    mov [rdi+jmp_buf.rbp], rbp
    mov [rdi+jmp_buf.r12], r12
    mov [rdi+jmp_buf.r13], r13
    mov [rdi+jmp_buf.r14], r14
    mov [rdi+jmp_buf.r15], r15
    lea rax, [rsp+8]
    mov [rdi+jmp_buf.rsp], rax
    xor rax, rax
    ret

; rdi Pointer to jmp_buf
; rsi Pointer to val
longjmp:
    mov rax, 1
    test rsi, rsi
    cmovnz rax, rsi
    mov rbp, [rdi+jmp_buf.rbp] 
    mov r12, [rdi+jmp_buf.r12] 
    mov r13, [rdi+jmp_buf.r13] 
    mov r14, [rdi+jmp_buf.r14] 
    mov r15, [rdi+jmp_buf.r15] 
    lea rax, [rsp+8]
    mov [rdi+jmp_buf.rsp], rax
    mov rdi, [rdi+jmp_buf.rip]
    jmp rdi
