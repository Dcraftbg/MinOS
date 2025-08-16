#include "gdt.h"
#include "string.h"
#include "kernel.h"
#include "kpanic.h"

void reload_gdt(void) {
    volatile GDTDescriptor descriptor;
    descriptor.size = (PAGE_SIZE-1);
    descriptor.addr = &kernel.gdt;
    __asm__ volatile (
        "lgdt (%0)"
        :
        : "r" (&descriptor)
    );
}
void init_gdt(void) {
    memset(&kernel.gdt, 0, PAGE_SIZE);
    kernel.gdt.null = 0;
    kernel.gdt.kernelCode = 0x0020980000000000; // Magic value for us. I'm too lazy to use macros. Read the docs instead
    kernel.gdt.kernelData = 0x0020920000000000; 
    kernel.gdt.userCode   = 0x0020F80000000000;
    kernel.gdt.userData   = 0x0020F20000000000; 
    memset(kernel.gdt.tss, 0, sizeof(kernel.gdt.tss)); // NOTE: Initialised later
    reload_gdt();
    kernel_reload_gdt_registers();
    kernel.tss.rsp0 = KERNEL_STACK_PTR;
}
