#pragma once
#include "../ps2utils.h"
extern Device ps2keyboard_device;
void idt_ps2_keyboard_handler();

intptr_t ps2keyboard_init();
