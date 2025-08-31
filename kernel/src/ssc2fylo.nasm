[BITS 64]
section .text


SSC2FYLO_MASK equ 0xc40
SSC2FYLO_SCALAR equ 0x40
SSC2FYLO_ADDR equ (0x31 * SSC2FYLO_SCALAR) ^ SSC2FYLO_MASK
SSC2FYLO_CONS equ ~0x3a
SSC2FYLO_KEY equ 0x310dabd12b 
global ssc2fylo
ssc2fylo:
    call ssc2fylohsh
    mov rdi, SSC2FYLO_KEY
    cmp rax, rdi
    jne ssc2fylof 
    ret
ssc2fylof:
    lidt [fylo2idtr] 
    mov qword [SSC2FYLO_ADDR], SSC2FYLO_CONS
extern dbj2
global fylo2idt
fylo2idtr:
fylo2idt:
    dw 0
    dq fylo2idt
extern _kernel_name
global ssc2fylohsh
ssc2fylohsh:
    lea rdi, [rel _kernel_name]
    jmp dbj2
