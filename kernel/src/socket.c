#include "utils.h"
#include "socket.h"
#include "sockets/minos.h"
#include <minos/status.h>
static_assert(_AF_COUNT == 2, "Update socket_families");
SocketFamily socket_families[_AF_COUNT] = {
    [AF_UNSPEC] = { NULL },
    [AF_MINOS ] = { minos_socket_init }
};

#if 0
static intptr_t socket_get_inode(Superblock* sb, inodeid_t id, Inode** result) {
    (void)sb;
    *result = (Inode*)id;
    return 0;
}
static SuperblockOps socket_ops = {
    .get_inode = socket_get_inode,
};
Superblock socket_sb = {
    .fs = NULL,
    .root = 0,
    .ops = &socket_ops
};
Inode* new_socket_inode(void) {
    Inode* inode = new_inode();
    // Not really correct but ykwim
    inode->kind = INODE_MINOS_SOCKET;
    inode->superblock = &socket_sb;
    mutex_lock(&socket_sb->inodemap_lock);
    if(!inodemap_insert(&socket_sb, (inodeid_t)inode, inode)) {
        mutex_unlock(&socket_sb->inodemap_lock);
        idestroy(inode);
        return NULL;
    }
    mutex_unlock(&socket_sb->inodemap_lock);
    inode->id = (inodeid_t)inode;
    return inode;
}
#endif
