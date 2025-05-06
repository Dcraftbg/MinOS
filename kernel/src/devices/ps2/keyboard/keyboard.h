#pragma once
#include "../ps2.h"
typedef struct Inode Inode;
extern Inode* ps2_keyboard_device;
void idt_ps2_keyboard_handler();
void init_ps2_keyboard();
