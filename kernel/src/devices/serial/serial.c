#include "serial.h"
#include "../../charqueue.h"
#include "../../pic.h"
// TODO: More formal way of registering interrupts
#include "../../arch/x86_64/idt.h"
#include "../../kernel.h"
#include "../../log.h"
static FsOps serialOps = {0};
static InodeOps inodeOps = {0};
static Device* serial0_device=NULL;
extern void idt_serial_handler();
#define COM_PORT   0x3f8
#define COM_5      (COM_PORT+5)
#define COM_STATUS COM_5
#define COM_INT_ENABLE_PORT   (COM_PORT+1)
void serial_handler() {
    if(!serial0_device) return;
    size_t retries = 10;
    while(retries && ((inb(COM_STATUS) & 0x01) == 0)) retries--;
    uint8_t key = inb(COM_PORT);
    charqueue_push((CharQueue*)serial0_device->private, key);
    pic_end(4);
}
static intptr_t serial_open(struct Inode* this, VfsFile* file, fmode_t mode) {
    file->ops = &serialOps;
    file->private = this->private;
    return 0;
}
static intptr_t init_inode(struct Device* this, struct Inode* inode) {
    inode->ops = &inodeOps;
    inode->private = this->private;
    return 0;
}
static intptr_t serial_dev_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
    (void)file;
    (void)offset; 
    for(size_t i = 0; i < size; ++i) {
        serial_print_chr(((const uint8_t*)buf)[i]);
    }
    return size;
}
static intptr_t serial_dev_read(VfsFile* file, void* buf, size_t size, off_t offset) {
    (void)offset;
    for(size_t i = 0; i < size; ++i, ++buf) {
        uint32_t key;
        if(!charqueue_pop((CharQueue*)file->private, &key)) return i;
        assert(key < 256 && "UTF8 support for serial");
        ((uint8_t*)buf)[0] = key;
    }
    return size;
}
intptr_t serial_dev_init() {
    memset(&serialOps, 0, sizeof(serialOps));
    inodeOps.open = serial_open;
    serialOps.write = serial_dev_write;
    serialOps.read  = serial_dev_read;
    idt_register(0x24, idt_serial_handler, IDT_TRAP_TYPE);
    outb(COM_INT_ENABLE_PORT, 0x01); // Enable received data available interrupt
    pic_clear_mask(4);
    return 0;
}
intptr_t serial_device_create(Device* device) {
    device->ops = &inodeOps;
    device->init_inode = init_inode;
    uint32_t* addr = (uint32_t*)kernel_malloc(PAGE_SIZE);
    if(!addr) return -NOT_ENOUGH_MEM;
    device->private = charqueue_new(addr, (PAGE_SIZE/sizeof(uint32_t))-1);
    if(!device->private) {
        kernel_dealloc(addr, PAGE_SIZE);
        return -NOT_ENOUGH_MEM;
    }
    if(!serial0_device) serial0_device = device;
    return 0;
}
