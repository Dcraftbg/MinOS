%include "asmstd.inc"
section .text
; Accepts:
;   rdi = task
;   rsi = task
;   rdx = frame 
extern fork

; Accepts:
;   rdi = task
;   rsi = task
global fork_trampoline
fork_trampoline:
    xchg bx, bx
    mov rax, [rsp]
    lea rdx, [rsp+8]
    mov rcx, KERNEL_CS ; cs
    push rcx
    pushfq
    push rdx
    mov rcx, KERNEL_SS ; ss
    push rcx
    irq_push_regs
    mov rdx, rsp
    call fork
    xchg bx, bx
    add rsp, 4*PTR_SIZE + IRQ_REGS_SIZE
    ret


