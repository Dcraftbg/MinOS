#include "log.h"
#include <minos/status.h>
#include "devices.h"
#include "./devices/serial/serial.h"
#include "./devices/ps2/ps2.h"
#include "./devices/fb/fb.h"

void init_devices() {
    intptr_t e;
    serial_dev_init();
    Device* device = (Device*)cache_alloc(kernel.device_cache);
    if(device) {
        if((e=serial_device_create(device)) < 0) {
            cache_dealloc(kernel.device_cache, device);
            kwarn("Failed to create serial0: %s", status_str(e));
        } else {
            if((e=vfs_register_device("serial0", device)) < 0) {
                cache_dealloc(kernel.device_cache, device);
                kwarn("Failed to register serial0: %s", status_str(e));
            }
        }
    } else {
        kwarn("Failed to allocate serial0 cache");
    }
    if((e=init_fb_devices()) < 0) {
        kwarn("Failed to initialise fb devices: %s", status_str(e));
    }
    init_ps2();
}

