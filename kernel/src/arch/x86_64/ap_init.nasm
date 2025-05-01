[BITS 64]
global ap_init
extern ap_main
struc limine_smp_info
   .processor_id   resd 1
   .lapic_id       resd 1
                   resq 1
   .goto_address   resq 1
   .extra_argument resq 1
endstruc
; RDI => limine_smp_info
ap_init:
    mov rsp, [rdi + limine_smp_info.extra_argument]
    call ap_main
loop:
    hlt
    jmp loop
