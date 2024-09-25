#include "idt.h"
#include "string.h"
#include "kernel.h"
void init_idt() {
    kernel.idt = (IDT*)kernel_malloc(PAGE_SIZE);
    if(!kernel.idt) {
        printf("ERROR: Ran out of memory for IDT");
        kabort();
    } 
    memset(kernel.idt, 0, PAGE_SIZE);
    IDTDescriptor idt_descriptor = {0};
    idt_descriptor.size = PAGE_SIZE-1;
    idt_descriptor.addr = (uint64_t)kernel.idt;
    __asm__ volatile(
        "lidt (%0)"
        :
        : "r" (&idt_descriptor)
    );
}
void idt_pack_entry(IDTEntry* result, IDTHandler_t handler, uint8_t typ) {
    uint64_t offset = (uint64_t)handler;
    result->base_low = offset & 0xFFFF;
    result->segment = GDT_KERNELCODE;
    result->tag = ((uint16_t)typ) << 8;
    result->base_middle = (offset >> 16) & 0xFFFF;
    result->base_high = (offset >> 32);
    result->reserved = 0;
}
