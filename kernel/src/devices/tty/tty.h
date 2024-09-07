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
intptr_t create_tty_device_display(size_t display, const char* keyboard, Device* device);
void destroy_tty_device(Device* device);
void init_tty();
