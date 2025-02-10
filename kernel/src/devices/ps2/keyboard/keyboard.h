#pragma once
#include <vfs.h>
#include "../ps2.h"
extern Device ps2keyboard_device;
void idt_ps2_keyboard_handler();
void init_ps2_keyboard();
