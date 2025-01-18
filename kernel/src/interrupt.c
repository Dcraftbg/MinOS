#include "interrupt.h"
#include "kernel.h"


intptr_t irq_reserve(size_t irq) {
    return kernel.interrupt_controller->reserve(kernel.interrupt_controller, irq);
}
void irq_clear(size_t irq) {
    kernel.interrupt_controller->set_mask(kernel.interrupt_controller, irq, 0);
}
void irq_mask(size_t irq) {
    kernel.interrupt_controller->set_mask(kernel.interrupt_controller, irq, 1);
}
void irq_eoi(size_t irq) {
    kernel.interrupt_controller->eoi(kernel.interrupt_controller, irq);
}

// Platform specific:
#include "pic.h"
#include <arch/x86_64/idt.h>
intptr_t irq_register(size_t irq, void (*handler)(), irq_flags_t flags) {
    intptr_t e;
    if((e=irq_reserve(irq)) < 0)
        return e;
    idt_register(e, handler, flags & IRQ_FLAG_FAST ? IDT_INTERRUPT_TYPE : IDT_TRAP_TYPE);
    return e;
}
