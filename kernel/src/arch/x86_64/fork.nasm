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
on_fork:
    xor rax, rax
    ret
fork_trampoline:
    push on_fork
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rdx, rsp
    call fork
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    add rsp, 8
    ret
