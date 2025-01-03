#pragma once
#include "pic.h"
static void irq_eoi(size_t irq) {
    pic_end(irq);
}
