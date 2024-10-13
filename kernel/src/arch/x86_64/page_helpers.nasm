[BITS 64]
section .text
global invalidate_full_page_table
invalidate_full_page_table:
    mov rax, cr3
    mov cr3, rax
    ret
