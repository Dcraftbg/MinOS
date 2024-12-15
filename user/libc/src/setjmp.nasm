[BITS 64]

section .text

global setjmp
struc jmp_buf
    .rip resq 1
    .rsp resq 1
endstruc
; rdi Pointer to jmp_buf 
setjmp:
    lea rax, [rsp-8]
    mov [rdi+jmp_buf.rip], rax
    mov rax, rsp
    mov [rdi+jmp_buf.rsp], rax
    xor rax, rax
    ret

; rdi Pointer to jmp_buf
; rsi Pointer to val
longjmp:
    mov rax, 1
    test rsi, rsi
    cmovnz rax, rsi

    mov rsp, [rdi+jmp_buf.rsp]
    mov rdi, [rdi+jmp_buf.rip]
    jmp rdi
