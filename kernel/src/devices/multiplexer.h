#pragma once
#include <vfs.h>
#include <sync/rwlock.h>
typedef struct {
    RwLock lock;
    struct list_head list;
} Multiplexer;
void multiplexer_add(Multiplexer* mp, Inode* inode);
extern Multiplexer keyboard_mp;
extern Multiplexer mouse_mp;
intptr_t init_multiplexers(void);
