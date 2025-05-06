#pragma once
#include "../ps2.h"
extern struct Inode* ps2_mouse_device;
void idt_ps2_mouse_handler();
intptr_t init_ps2_mouse(void);
