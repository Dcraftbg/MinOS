#pragma once
#include "../../vfs.h"
#include <minos/status.h>
#include "../../serial.h"
#include "../../string.h"
#include "../../slab.h"
#include "../../framebuffer.h"
#include "../../bootutils.h"
#include <stdint.h>
#include "../vga/vga.h"
intptr_t create_tty_device(const char* display, const char* keyboard, Device* device);
intptr_t tty_init();
void destroy_tty_device(Device* device);
