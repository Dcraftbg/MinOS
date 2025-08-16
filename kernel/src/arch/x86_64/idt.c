#include "idt.h"
#include "string.h"
#include "kernel.h"
#include "log.h"
#include "task_regs.h"
#include "interrupt.h"

void reload_idt() {
#ifdef GLOBAL_STORAGE_GDT_IDT
    IDT* idt = &kernel.idt;
#else
    IDT* idt = kernel.idt;
#endif
    volatile IDTDescriptor idt_descriptor;
    idt_descriptor.size = PAGE_SIZE-1;
    idt_descriptor.addr = (uint64_t)idt;
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
    idt_register(e, _irq_vectors[e], flags & IRQ_FLAG_FAST ? IDT_INTERRUPT_TYPE : IDT_TRAP_TYPE);
    return e;
}
void init_idt() {
#ifdef GLOBAL_STORAGE_GDT_IDT
    IDT* idt = &kernel.idt;
#else
    IDT* idt = (IDT*)kernel_malloc(PAGE_SIZE);
    kernel.idt = idt;
    if(!kernel.idt) kpanic("Ran out of memory for IDT");
#endif
    memset(idt, 0, 4096);
    reload_idt();
    for(size_t i = 0; i < 256; ++i) {
        idt_register(i, _irq_vectors[i], i == 0x80 ? IDT_SOFTWARE_TYPE : IDT_INTERRUPT_TYPE);
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
