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
static bool chrtty_is_readable(Inode* device) {
    return inode_is_readable((Inode*)(((Tty*)device)->priv));
}
static bool chrtty_is_writeable(Inode* device) {
    return inode_is_writeable((Inode*)(((Tty*)device)->priv));
}
static intptr_t chrtty_deinit(Tty* device) {
    idrop(device->priv);
    device->priv = NULL;
    return 0;
}
static intptr_t chrtty_getsize(Tty*, TtySize* size) {
    size->width = 200; // 80;
    size->height = 44; // 24;
    return 0;
}
static InodeOps inodeOps = {
    .write = tty_write,
    .read  = tty_read,
    .is_readable = chrtty_is_readable,
    .is_writeable = chrtty_is_writeable,
    .ioctl = tty_ioctl,
};
Tty* chrtty_new(Inode* inode) {
    Tty* device = tty_new();
    device->priv = inode;
    device->getchar = chrtty_getchar;
    device->putchar = chrtty_putchar;
    device->getsize = chrtty_getsize;
    device->deinit = chrtty_deinit;
    tty_init(device, tty_cache);
    device->inode.ops = &inodeOps;
    return device;
}
