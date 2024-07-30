#include "devices.h"
#include "./devices/serial/serial.h"
#include "./devices/vga/vga.h"
void init_devices() {
    serialDevice.init();
    vfs_register_device("serial0", &serialDevice);
}

