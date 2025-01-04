#pragma once
#include <stddef.h>
#include <stdint.h>
// TODO: Expose the Low vs High, Level vs Edge triggers
typedef struct IntController IntController;
struct IntController {
    // Returns interrupt vector
    intptr_t (*reserve)(IntController* c, size_t irq);
    void (*eoi)(IntController* c, size_t irq);
    void (*clear)(IntController* c, size_t irq);
    void *priv;
};
typedef int irq_flags_t;
enum {
    IRQ_FLAG_FAST = 0b1
};
intptr_t irq_register(size_t irq, void (*handler)(), irq_flags_t flags);
// Wrappers around interrupt_controller functions
intptr_t irq_reserve(size_t irq);
void     irq_clear(size_t irq);
void     irq_eoi(size_t irq);
