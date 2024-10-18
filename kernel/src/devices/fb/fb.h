#pragma once
#include "../../vfs.h"
#include <minos/status.h>
#include "../../string.h"
#include "../../mem/slab.h"
#include "../../framebuffer.h"
#include "../../bootutils.h"
#include <stdint.h>
void destroy_fb_device(Device* device);
intptr_t init_fb_devices();

