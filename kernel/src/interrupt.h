#pragma once
#include <stddef.h>
void irq_eoi(size_t irq);
typedef int irq_flags_t;
enum {
    IRQ_FLAG_FAST = 0b1
};
void irq_register(size_t irq, void (*handler)(), irq_flags_t flags);
void irq_clear(size_t irq);
