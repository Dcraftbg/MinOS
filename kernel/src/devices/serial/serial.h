#pragma once
#include "../../vfs.h"
#include <minos/status.h>
#include "../../serial.h"
#include "../../string.h"
#include <stdint.h>
extern Device serialDevice;
intptr_t serial_dev_init();
