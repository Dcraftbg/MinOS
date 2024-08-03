#pragma once
#include "../../vfs.h"
#include <status.h>
#include "../../serial.h"
#include "../../string.h"
#include "../../slab.h"
#include "../../framebuffer.h"
#include "../../bootutils.h"
#include <stdint.h>
extern Device vgaDevice;
// intptr_t vga_draw_codepoint(Framebuffer* fm, size_t x, size_t y, int codepoint);
intptr_t vga_init();
intptr_t create_vga_device(size_t id, Device* device);
void destroy_vga_device(Device* device);
