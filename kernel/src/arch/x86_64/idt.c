#include "idt.h"
#include "string.h"
#include "kernel.h"
#include "log.h"
#include "task_regs.h"
#include "interrupt.h"
#include "gdt.h"
#include "memory.h"
#include "utils.h"

#define idt_register(what, handler, typ) idt_pack_entry(&kernel.idt.inner[what], handler, typ);
void reload_idt() {
    volatile IDTDescriptor idt_descriptor;
    idt_descriptor.size = PAGE_SIZE-1;
    idt_descriptor.addr = (uint64_t)&kernel.idt;
    __asm__ volatile(
        "lidt (%0)"
        :
        : "r" (&idt_descriptor)
    );
}
// NOTE: generated Automatically in assembly (vectors.nasm)
extern IDTHandler_t _irq_vectors[];
static IrqHandler irq_handlers[256];
void irq_handle(TaskRegs* regs) {
    irq_handlers[regs->irq](regs);
}
void irq_set_handler(size_t id, IrqHandler handler) {
    irq_handlers[id] = handler;
}
intptr_t irq_register(size_t irq, IrqHandler handler, irq_flags_t flags) {
    intptr_t e;
    if((e=irq_reserve(irq)) < 0)
        return e;
    irq_set_handler(e, handler);
    idt_register(e, _irq_vectors[e], flags & IRQ_FLAG_TRAP ? IDT_TRAP_TYPE : IDT_INTERRUPT_TYPE);
    return e;
}
void init_idt() {
    memset(&kernel.idt, 0, 4096);
    reload_idt();
    for(size_t i = 0; i < 256; ++i) {
        uint8_t type = IDT_INTERRUPT_TYPE;
        if(i < 32) type = IDT_TRAP_TYPE;
        else if(i == 0x80) type = IDT_SOFTWARE_TYPE;
        idt_register(i, _irq_vectors[i], type);
    }
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
