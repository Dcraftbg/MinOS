#include "gdt.h"
#include "string.h"
#include "kernel.h"
void init_gdt() {
#ifdef GLOBAL_STORAGE_GDT_IDT
    GDT* gdt = &kernel.gdt;
#else
    GDT* gdt = (GDT*)kernel_malloc(PAGE_SIZE);
    kernel.gdt = gdt;
    if(!kernel.gdt) {
        printf("ERROR: Ran out of memory for GDT");
        kabort();
    }
#endif
    memset(gdt, 0, PAGE_SIZE);
    gdt->null = 0;
    gdt->kernelCode = 0x0020980000000000; // Magic value for us. I'm too lazy to use macros. Read the docs instead
    gdt->kernelData = 0x0020920000000000; 
    gdt->userCode   = 0x0020F80000000000;
    gdt->userData   = 0x0020F20000000000; 
    memset(gdt->tss, 0, sizeof(gdt->tss)); // NOTE: Initialised later
    volatile GDTDescriptor descriptor;
    descriptor.size = (PAGE_SIZE-1);
    descriptor.addr = gdt;
    __asm__ volatile (
        "lgdt (%0)"
        :
        : "r" (&descriptor)
    );
    kernel_reload_gdt_registers();
    kernel.tss.rsp0 = KERNEL_STACK_PTR;
}
