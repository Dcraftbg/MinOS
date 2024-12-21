#pragma once
#include "../../devices/tty/tty.h"

#define VGA_FG 0xececec
#define VGA_BG 0x212121
Tty* fbtty_new(Inode* keyboard, size_t framebuffer_id);
intptr_t init_fbtty(void);
void deinit_fbtty(void);
