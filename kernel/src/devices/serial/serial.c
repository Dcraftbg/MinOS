#include "./serial.h"
static FsOps serialOps = {0};

static intptr_t serial_open(struct Device* this, VfsFile* file, fmode_t mode) {
     if(mode != MODE_WRITE) return -UNSUPPORTED;
     return 0;
}
static intptr_t serial_dev_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
     (void)file;
     (void)offset; 
     for(size_t i = 0; i < size; ++i) {
        serial_print_u8(((const uint8_t*)buf)[i]);
     }
     return size;
}
static intptr_t serial_dev_init() {
     memset(&serialOps, 0, sizeof(serialOps));
     serialOps.write = serial_dev_write;
     return 0;
}

Device serialDevice = {
     &serialOps,
     NULL,
     serial_open,
     serial_dev_init,
     NULL,
};
