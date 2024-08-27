#pragma once
#include <stdatomic.h>
#define PIT_FREQUENCY 1193182
static void pit_set_count(size_t count) {
    disable_interrupts();
    size_t hz = PIT_FREQUENCY / count;
    outb(0x43, 0x36);
    outb(0x40,  hz       & 0xFF);
    outb(0x40, (hz >> 8) & 0xFF);
    enable_interrupts();
}
typedef struct {
    atomic_size_t ticks;
} PitInfo;
