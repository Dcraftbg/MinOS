#include "interrupt.h"
#include "kernel.h"
// Platform specific:
#include "pic.h"
#include <arch/x86_64/idt.h>
void irq_eoi(size_t irq) {
    pic_end(irq);
}
// TODO: I should do +0x20 because irq_register is NOT going to be used by exception definitions as they're platform specific
// and it makes irq_eoi and irq_clear inconsistent
void irq_register(size_t irq, void (*handler)(), irq_flags_t flags) {
    idt_register(irq, handler, flags & IRQ_FLAG_FAST ? IDT_INTERRUPT_TYPE : IDT_TRAP_TYPE);
}
void irq_clear(size_t irq) {
    pic_clear_mask(irq);
}
