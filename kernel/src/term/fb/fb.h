#pragma once
#include "../../devices/tty/tty.h"
#include "../../../../config.h"
#ifdef JAKE_COLORSCHEME
#define VGA_FG 0xa4ada6
#define VGA_BG 0x24283b
#else
#define VGA_FG 0xececec
#define VGA_BG 0x212121
#endif
Tty* fbtty_new(Inode* keyboard, size_t framebuffer_id);
intptr_t init_fbtty(void);
void deinit_fbtty(void);
