#include "pic.h"
#include "port.h"
#include "utils.h"
#include "kernel.h"
#include <minos/status.h>

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_CMD   PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_CMD   PIC2
#define PIC2_DATA (PIC2+1)
void init_pic() {
    outb(PIC1, 0x11);
    outb(PIC2, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0b1);
    outb(PIC2_DATA, 0b1);

    outb(PIC1_DATA, 0b11111111);
    outb(PIC2_DATA, 0b11111111);
    kernel.interrupt_controller = &pic_controller;
}

void pic_eoi(IntController* _, size_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
}

void pic_clear(IntController* _, size_t irq) {
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
intptr_t pic_reserve(IntController* _, size_t irq) {
    if(irq >= 16) return -UNSUPPORTED;
    return irq;
}
IntController pic_controller = {
    .eoi = pic_eoi,
    .clear = pic_clear,
    .reserve = pic_reserve,
    .priv = NULL,
};
