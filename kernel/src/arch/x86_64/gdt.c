#include "gdt.h"
#include "string.h"
#include "kernel.h"
void init_gdt() {
    kernel.gdt = (GDT*)kernel_malloc(PAGE_SIZE);
    if(!kernel.gdt) {
        printf("ERROR: Ran out of memory for GDT");
        kabort();
    }
    memset(kernel.gdt, 0, PAGE_SIZE);
    kernel.gdt->null = 0;
    kernel.gdt->kernelCode = 0x0020980000000000; // Magic value for us. I'm too lazy to use macros. Read the docs instead
    kernel.gdt->kernelData = 0x0020920000000000; 
    kernel.gdt->userCode   = 0x0020F80000000000;
    kernel.gdt->userData   = 0x0020F20000000000; 
    kernel.gdt->tss = 0; // NOTE: Initialised later
    GDTDescriptor descriptor = {0};
    descriptor.size = (PAGE_SIZE-1);
    descriptor.addr = kernel.gdt;
    __asm__ volatile (
        "lgdt (%0)"
        :
        : "r" (&descriptor)
    );
    kernel_reload_gdt_registers();
    kernel.tss.rsp0 = KERNEL_STACK_PTR;
}
