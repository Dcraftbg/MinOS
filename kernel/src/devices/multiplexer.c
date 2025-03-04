#include "multiplexer.h"
// FIXME: Multiplexer cleanup logic.
void multiplexer_add(Multiplexer* mp, Inode* inode) {
    rwlock_begin_write(&mp->lock);
    list_append(&inode->list, &mp->list);
    rwlock_end_write(&mp->lock);
}
Multiplexer keyboard_mp = { 0 };
Multiplexer mouse_mp = { 0 };
static intptr_t multiplexer_read(Inode* file, void* buf, size_t size, off_t offset) {
    (void)offset;
    Multiplexer* mp = file->priv;
    size_t n = 0;
    intptr_t e;
    rwlock_begin_read(&mp->lock);
    for(struct list *head = mp->list.next; head != &mp->list && n < size; head = head->next) {
        Inode* inode = (Inode*)head;
        // TODO: Think about the error logic here.
        // I don't know if this is the best as it kinda starves the devices down the chain so I don't really know
        if((e=inode_read(inode, buf + n, size - n, 0)) < 0) {
            rwlock_end_read(&mp->lock);
            if(n == 0) return e;
            return n;
        }
        n += (size_t)e;
    }
    rwlock_end_read(&mp->lock);
    return n;
}
static bool multiplexer_is_readable(Inode* file) {
    Multiplexer* mp = file->priv;
    rwlock_begin_read(&mp->lock);
    for(struct list *head = mp->list.next; head != &mp->list; head = head->next) {
        Inode* inode = (Inode*)head;
        if(inode_is_readable(inode)) {
            rwlock_end_read(&mp->lock);
            return true;
        }
    }
    rwlock_end_read(&mp->lock);
    return false;
}
static InodeOps inodeOps = {
    .read = multiplexer_read,
    .is_readable = multiplexer_is_readable,
};
static intptr_t init_inode(Device* me, Inode* inode) {
    inode->priv = me->priv;
    inode->ops = &inodeOps;
    return 0;
}
intptr_t init_multiplexers(void) {
    list_init(&keyboard_mp.list);
    rwlock_init(&keyboard_mp.lock);
    list_init(&mouse_mp.list);
    rwlock_init(&mouse_mp.lock);
    static Device keyboard = {
        .priv = &keyboard_mp,
        .init_inode = init_inode
    };
    static Device mouse = {
        .priv = &mouse_mp,
        .init_inode = init_inode
    };
    intptr_t e;
    if((e = vfs_register_device("keyboard", &keyboard)) < 0) return e;
    if((e = vfs_register_device("mouse", &mouse)) < 0) return e;
    return 0;
}
