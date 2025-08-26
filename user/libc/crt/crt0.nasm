[BITS 64]
global _start
extern __libc_start_main
extern main
_start:
    xor rbp, rbp
    lea r8, [rel main WRT ..plt]
    call __libc_start_main WRT ..plt
    hlt
section .note.GNU-stack
