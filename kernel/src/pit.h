#pragma once
#include <stddef.h>
#include "port.h"
#define PIT_FREQUENCY 1193182
static void pit_set_hz(size_t hz) {
    disable_interrupts();
    size_t count = PIT_FREQUENCY / hz;
    outb(0x43, 0x36);
    outb(0x40,  count       & 0xFF);
    outb(0x40, (count >> 8) & 0xFF);
    enable_interrupts();
}
