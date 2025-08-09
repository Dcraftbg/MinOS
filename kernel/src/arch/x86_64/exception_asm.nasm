[BITS 64]
global _exception_handler
extern exception_handler
_exception_handler:
    mov rsi, cr2
    jmp exception_handler
