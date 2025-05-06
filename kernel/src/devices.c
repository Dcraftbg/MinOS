#include "log.h"
#include <minos/status.h>
#include "devices.h"
#include "./devices/multiplexer.h"
#include "./devices/serial/serial.h"
#include "./devices/ps2/ps2.h"
#include "./devices/ptm/ptm.h"
#include "./devices/fb/fb.h"

void init_devices() {
    intptr_t e;
    if((e=init_multiplexers()) < 0) {
        kwarn("Failed to initialise multiplexers: %s", status_str(e));
    }
    if((e=init_serial_device()) < 0) {
        kwarn("Failed to initialise serial0: %s", status_str(e));
    }
    if((e=init_fb_devices()) < 0) {
        kwarn("Failed to initialise fb devices: %s", status_str(e));
    }
    init_ps2();
    if((e=init_ptm()) < 0) {
        kwarn("Failed to initialise ptm device: %s", status_str(e));
    }
}

