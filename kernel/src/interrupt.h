#pragma once

static void irq_eoi(size_t irq) {
    pic_end(irq);
}
