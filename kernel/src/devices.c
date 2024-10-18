#include "log.h"
#include <minos/status.h>
#include "devices.h"
#include "./devices/serial/serial.h"
#include "./devices/ps2/ps2.h"
#include "./devices/fb/fb.h"

void init_devices() {
    intptr_t e;
    serial_dev_init();
    vfs_register_device("serial0", &serialDevice);
    if((e=init_fb_devices()) < 0) {
        kwarn("Failed to initialise fb devices: %s", status_str(e));
    }
    init_ps2();
}

