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
