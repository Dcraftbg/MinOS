#include "chr.h"
#include "../../log.h"
static uint32_t chrtty_getchar(Tty* device) {
    intptr_t e;
    Inode* inode = device->priv;
    char code;
    for(;;) {
        if((e=inode_read(inode, &code, sizeof(char), 0)) < 0) {
            kerror("(chrtty:%p %lu) Failed to read on character device: %s", device, inode->id, status_str(e));
            return 0;
        } else if (e > 0) break;
    }
    return (uint32_t)code;
}
static void chrtty_putchar(Tty* device, uint32_t code) {
    intptr_t e;
    Inode* inode = device->priv;
    if((e=inode_write(inode, &code, sizeof(char), 0)) < 0) {
        kerror("(chrtty:%p %lu) Failed to write on character device: %s", device, inode->id, status_str(e));
        return;
    }
}
static intptr_t chrtty_deinit(Tty* device) {
    idrop(device->priv);
    device->priv = NULL;
    return 0;
}
Tty* chrtty_new(Inode* inode) {
    Tty* device = tty_new();
    device->priv = inode;
    device->getchar = chrtty_getchar;
    device->putchar = chrtty_putchar;
    device->deinit = chrtty_deinit;
    return device;
}
