#include "devices.h"
#include "./devices/serial/serial.h"
#include "./devices/vga/vga.h"
#include "./devices/ps2/ps2.h"
void init_devices() {
    serial_dev_init();
    vfs_register_device("serial0", &serialDevice);
    init_ps2();
}

