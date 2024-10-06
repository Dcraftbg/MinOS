#include "idt.h"
#include "string.h"
#include "kernel.h"
void init_idt() {
#ifdef GLOBAL_STORAGE_GDT_IDT
    IDT* idt = &kernel.idt;
#else
    IDT* idt = (IDT*)kernel_malloc(PAGE_SIZE);
    kernel.idt = idt;
    if(!kernel.idt) {
        printf("ERROR: Ran out of memory for IDT");
        kabort();
    } 
#endif
    memset(idt, 0, PAGE_SIZE);
    IDTDescriptor idt_descriptor = {0};
    idt_descriptor.size = PAGE_SIZE-1;
    idt_descriptor.addr = (uint64_t)idt;
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
