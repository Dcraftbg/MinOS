#include "pic.h"
void init_pic() {
    outb(PIC2, 0x11);
    outb(PIC1, 0x11);

    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x20);

    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x04);

    outb(PIC2_DATA, 0);
    outb(PIC1_DATA, 0);

    outb(PIC1_DATA, 0b11111111);
    outb(PIC2_DATA, 0b11111111);
}
