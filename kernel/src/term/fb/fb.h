#pragma once
#include <../../config.h>
#include <stddef.h>
#include <stdint.h>

#ifdef JAKE_COLORSCHEME
#define VGA_FG 0xa4ada6
#define VGA_BG 0x24283b
#else
#define VGA_FG 0xececec
#define VGA_BG 0x212121
#endif

typedef struct Inode Inode;
typedef struct Tty Tty;
Tty* fbtty_new(Inode* keyboard, size_t framebuffer_id);
intptr_t init_fbtty(void);
void deinit_fbtty(void);
