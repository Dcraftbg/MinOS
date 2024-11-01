#pragma once
#include "port.h"
#include "utils.h"
#include <stdint.h>
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_CMD   PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_CMD   PIC2
#define PIC2_DATA (PIC2+1)
void init_pic();

static inline void pic_end(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
}
static void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t mask = inb(port) & ~(1 << irq);
    outb(port, mask);
}
