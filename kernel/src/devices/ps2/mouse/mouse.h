#pragma once
#include <vfs.h>
#include "../ps2.h"
extern Device ps2mouse_device;
void idt_ps2_mouse_handler();
intptr_t init_ps2_mouse(void);
