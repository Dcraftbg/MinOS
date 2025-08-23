#include "ptm.h"
#include <log.h>
#include <mem/slab.h>
#include <minos/ptm/ptm.h>
#include <devices/tty/tty.h>

#define MAX_PTTYS 256
typedef struct Ptty Ptty;
// Reusing 
// MinOSData
#include <sockets/minos.h>
struct Ptty {
    Inode inode;
    TtySize size;
    Inode* slave;
    atomic_size_t shared;
    MinOSData slave_ibuf; /*master_obuf*/
    Mutex slave_ibuf_lock;
    MinOSData slave_obuf; /*master_ibuf*/
    Mutex slave_obuf_lock;
};

static Cache* ptty_cache = NULL;  
static void ptty_drop(Ptty* ptty) {
    if(--ptty->shared == 0) {
        minos_data_free(&ptty->slave_ibuf);
        minos_data_free(&ptty->slave_obuf);
        cache_dealloc(ptty_cache, ptty);
    }
}
static intptr_t ptty_statx(Inode*, uint32_t mask, struct statx* stats) {
    if(mask & STATX_TYPE) {
        stats->stx_type = STX_TYPE_DEVICE;
        stats->stx_mask |= STATX_TYPE;
    }
    return 0;
}
static void ptty_cleanup(Inode* inode) {
    ptty_drop((Ptty*)inode);
}
// TODO: Separate Out. Just like how we should also separate out the MinOSData
// from MinOS sockets to reuse them in Ptty, Pipes, MinOS sockets, etc.
static size_t minos_data_consume(MinOSData* data, void* buf, size_t size) {
    if(size > data->len) size = data->len;
    memcpy(buf, data->addr, size);
    data->len -= size;
    memmove(data->addr, data->addr + size, data->len);
    return size;
}
static size_t minos_data_provide(MinOSData* data, const void* buf, size_t size) {
    if(size > data->cap-data->len) size = data->cap-data->len;
    memcpy(data->addr + data->len, buf, size);
    data->len += size;
    return size;
}
static bool ptty_master_is_readable(Inode* file) {
    Ptty* ptty = (Ptty*)file;
    if(mutex_try_lock(&ptty->slave_obuf_lock)) return false;
    size_t len = ptty->slave_obuf.len;
    mutex_unlock(&ptty->slave_obuf_lock);
    return len > 0;
}
static bool master_is_hungup(Inode* inode) {
    Ptty* ptty = (Ptty*)inode;
    if(ptty->shared == 1) return true;
    // FIXME: Potential race condition?
    // I'm not too sure. Like if the shared gets decremented in the time between this check and the return.
    // I guess it doesn't really matter but it might be UB if you're touching 
    return ptty->slave->shared == 1; 
}
static intptr_t ptty_master_read(Inode* file, void* buf, size_t size, off_t) {
    Ptty* ptty = (Ptty*)file;
    mutex_lock(&ptty->slave_obuf_lock);
    if(ptty->slave_obuf.len == 0) {
        mutex_unlock(&ptty->slave_obuf_lock);
        return -WOULD_BLOCK;
    }
    size = minos_data_consume(&ptty->slave_obuf, buf, size);
    mutex_unlock(&ptty->slave_obuf_lock);
    return size;
}
static intptr_t ptty_master_write(Inode* file, const void* buf, size_t size, off_t) {
    Ptty* ptty = (Ptty*)file;
    mutex_lock(&ptty->slave_ibuf_lock);
    intptr_t e;
    if((e=minos_data_reserve_at_least(&ptty->slave_ibuf, ptty->slave_ibuf.len + size)) < 0) {
        mutex_unlock(&ptty->slave_ibuf_lock);
        return e;
    }
    size = minos_data_provide(&ptty->slave_ibuf, buf, size);
    mutex_unlock(&ptty->slave_ibuf_lock);
    return size;
}
static intptr_t ptty_master_ioctl(Inode* file, Iop op, void* arg) {
    Ptty* ptty = (Ptty*)file;
    // FIXME: check arg
    if(op == TTY_IOCTL_SET_SIZE) ptty->size = *(TtySize*)arg;
    else return -UNSUPPORTED;
    return 0;
} 
static InodeOps master_inodeOps = {
    .write = ptty_master_write,
    .read = ptty_master_read,
    .ioctl = ptty_master_ioctl,
    .is_readable = ptty_master_is_readable,
    .is_hungup = master_is_hungup,
};
static Ptty* ptty_new(void) {
    Ptty* ptty = cache_alloc(ptty_cache);
    if(!ptty) return NULL;
    memset(ptty, 0, sizeof(*ptty));
    ptty->shared = 1;
    ptty->inode.ops = &master_inodeOps;
    ptty->inode.cache = ptty_cache;
    return ptty;
}
static bool ptty_slave_is_readable(Inode* file)  {
    Ptty* ptty = ((Tty*)file)->priv;
    if(mutex_try_lock(&ptty->slave_ibuf_lock)) return false;
    size_t len = ptty->slave_ibuf.len;
    mutex_unlock(&ptty->slave_ibuf_lock);
    return len > 0;
}
static uint32_t ptty_slave_getchar(Tty* tty) {
    Ptty* ptty = tty->priv;
    for(;;) {
        mutex_lock(&ptty->slave_ibuf_lock);
        if(ptty->slave_ibuf.len > 0) break; // <- Intentionally not unlock it here.
        mutex_unlock(&ptty->slave_ibuf_lock);
    }
    uint8_t c;
    minos_data_consume(&ptty->slave_ibuf, &c, 1);
    mutex_unlock(&ptty->slave_ibuf_lock);
    return c;
}
static void ptty_slave_putchar(Tty* tty, uint32_t code) {
    Ptty* ptty = tty->priv;
    mutex_lock(&ptty->slave_obuf_lock);
    intptr_t e;
    if((e=minos_data_reserve_at_least(&ptty->slave_obuf, ptty->slave_obuf.len + 1)) < 0) {
        mutex_unlock(&ptty->slave_obuf_lock);
        // TODO: Report error
        return;
    }
    minos_data_provide(&ptty->slave_obuf, &code, 1);
    mutex_unlock(&ptty->slave_obuf_lock);
}
static intptr_t ptty_slave_getsize(Tty* tty, TtySize* size) {
    Ptty* ptty = tty->priv;
    *size = ptty->size;
    return 0;
}
static intptr_t ptty_slave_deinit(Tty* tty) {
    ptty_drop((Ptty*)tty->priv);
    return 0;
}
static InodeOps slave_inodeOps = {
    .write = tty_write,
    .read = tty_read,
    .ioctl = tty_ioctl,
    .is_readable = ptty_slave_is_readable,
};
static Tty* ptty_slave_new(Ptty* ptty) {
    Tty* tty = tty_new();
    if(!tty) return NULL;
    ptty->shared++;
    tty->priv = ptty;
    tty->getchar = ptty_slave_getchar;
    tty->putchar = ptty_slave_putchar;
    tty->getsize = ptty_slave_getsize;
    tty_init(tty, tty_cache);
    tty->inode.ops = &slave_inodeOps;
    return tty;
}
// NOTE:
// I know its kind of confusing but pttys is used for both
// a Cache and storing the actual data
static Ptty* pttys[MAX_PTTYS];
static int ptty_pick(void) {
    for(size_t i = 0; i < MAX_PTTYS; ++i) {
        if(!pttys[i]) return i;
    }
    return -1;
}
static intptr_t ptm_ioctl(Inode* me, Iop op, void*) {
    intptr_t e;
    switch(op) {
    case PTM_IOCTL_MKPTTY: {
        int ptty_index = ptty_pick();
        if(ptty_index < 0) return -LIMITS;
        Ptty* ptty = ptty_new();
        if(!ptty) return -NOT_ENOUGH_MEM;
        if(!(pttys[ptty_index] = ptty)) {
            ptty_drop(ptty);
            return -NOT_ENOUGH_MEM;
        }
        Inode* slave = &ptty_slave_new(ptty)->inode;
        if(!slave) {
            idrop(&pttys[ptty_index]->inode);
            ptty_drop(ptty);
            pttys[ptty_index] = NULL;
            return -NOT_ENOUGH_MEM;
        }
        pttys[ptty_index]->slave = slave;
        char name[20];
        memcpy(name, "pts", 3);
        (name+3)[itoa(name+3, sizeof(name)-3-1, ptty_index)] = '\0';
        if((e=vfs_register_device(name, slave)) < 0) {
            idrop(&pttys[ptty_index]->inode);
            ptty_drop(ptty);
            pttys[ptty_index] = NULL;
            idrop(slave);
            return e;
        }
        return ptty_index;
    }
    }
    return -UNSUPPORTED;
}
static intptr_t ptm_get_dir_entries(Inode*, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes) {
    if(offset > MAX_PTTYS) return -INVALID_OFFSET; 
    if(offset == MAX_PTTYS) {
        *read_bytes = 0;
        return 0;
    }
    size_t read = 0;
    size_t n = 0;
    for(size_t i = offset; i < MAX_PTTYS && read+sizeof(DirEntry)+4+4 < size; ++i) {
        if(!pttys[i]) continue;
        DirEntry* entry = (DirEntry*)(((uint8_t*)entries)+read);
        entry->inodeid = i;
        entry->kind = STX_TYPE_DEVICE;
        memcpy(entry->name, "ptty", 4);
        read += entry->size = sizeof(DirEntry) + 4 + itoa(entry->name + 4, 4, i);
        n++;
    }
    *read_bytes = read;
    return n;
}
static intptr_t ptm_find(Inode*, const char* name, size_t namelen, Inode** result) {
    if(memcmp(name, "ptty", 4) != 0) return -NOT_FOUND;
    name += 4;
    namelen -= 4;
    if(namelen == 0) return -NOT_FOUND;
    size_t index = 0;
    for(size_t i = 0; i < namelen; ++i) {
        if(i > 3 || name[i] < '0' || name[i] > '9') return -NOT_FOUND;
        index = index * 10 + (name[i] - '0');
    }
    if(index >= MAX_PTTYS) return -NOT_FOUND;
    if(!pttys[index]) return -NOT_FOUND;
    *result = iget(&pttys[index]->inode);
    return 0;
}
static InodeOps inodeOps = {
    .ioctl = ptm_ioctl,
    .get_dir_entries = ptm_get_dir_entries,
    .find = ptm_find,
};
intptr_t init_ptm(void) {
    intptr_t e;
    ptty_cache = create_new_cache(sizeof(Ptty), "Ptty");
    Inode* ptm = new_inode();
    ptm->ops = &inodeOps;
    if((e=vfs_register_device("ptm", ptm)) < 0) {
        kerror("Failed to register ptm: %s", status_str(e));
        return e;
    }
    return 0;
}
