#pragma once
#include "../../vfs.h"
#include <minos/status.h>
#include "../../serial.h"
#include "../../string.h"
#include <stdint.h>
intptr_t serial_device_create(Device* device);
intptr_t serial_dev_init();
