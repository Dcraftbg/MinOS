[BITS 64]
section .text
global kernel_reload_gdt_registers
; align	  0x08,	  db	0x00
kernel_reload_gdt_registers:
   push rax
   push 0x08
   push .cs
   retfq
.cs:
   mov ax, 0x10
   mov ds, ax
   mov es, ax
   mov ss, ax

   xor ax, ax
   mov fs, ax
   mov gs, ax
   pop rax
   ret
