#define INODEMAP_DEFINE
#include <stddef.h>
struct Cache* hashpair_cache = NULL;
#include "kernel.h"
#include "vfs.h"
#include "./fs/tmpfs/tmpfs.h"
#include "mem/slab.h"
#include "string.h"
#include "debug.h"

Inode* new_inode() {
    Inode* inode = cache_alloc(kernel.inode_cache);
    if(inode) {
        memset(inode, 0, sizeof(*inode));
        list_init(&inode->list);
        inode->shared = 1;
    }
    return inode;
}
Inode* iget(Inode* inode) {
    inode->shared++;
    return inode;
}
void idrop(Inode* inode) {
    debug_assert(inode->shared);
    if(inode->shared == 1) {
        mutex_lock(&inode->superblock->inodemap_lock);
        assert(inodemap_remove(&inode->superblock->inodemap, inode->id));
        mutex_unlock(&inode->superblock->inodemap_lock);
        inode_cleanup(inode);
        cache_dealloc(kernel.inode_cache, inode);
        return;
    }
    inode->shared--;
}
static intptr_t sb_get_inode(Superblock* sb, inodeid_t id, Inode** inode) {
    assert(sb->ops->get_inode && "Superblock MUST implement get_inode");
    return sb->ops->get_inode(sb, id, inode);
}
intptr_t fetch_inode(Superblock* sb, inodeid_t id, Inode** result) {
    debug_assert(sb);
    intptr_t e;
    Inode** ref = NULL;
    mutex_lock(&sb->inodemap_lock);
    if((ref=inodemap_get(&sb->inodemap, id))) {
        *result = iget(*ref);
        mutex_unlock(&sb->inodemap_lock);
        return 0;
    }
    if((e=sb_get_inode(sb, id, result)) < 0) {
        mutex_unlock(&sb->inodemap_lock);
        return e;
    }
    if(!inodemap_insert(&sb->inodemap, id, *result)) {
        mutex_unlock(&sb->inodemap_lock);
        idrop(*result);
        return -NOT_ENOUGH_MEM;
    }
    mutex_unlock(&sb->inodemap_lock);
    return 0;
}
static Fs* get_rootfs() {
    return &tmpfs;
}
void init_vfs() {
    assert(kernel.inode_cache = create_new_cache(sizeof(Inode), "Inode"));
    assert(hashpair_cache = create_new_cache(sizeof(Pair_InodeMap), "Pair_InodeMap"));
    intptr_t e;
    Fs* rootfs = get_rootfs(); 
    if(rootfs->init && (e=rootfs->init(rootfs)) < 0) 
        kpanic("Could not initialise rootfs: %s", status_str(e));
    // @DEBUG Intentionally checking this on debug. Can safely be removed in release given
    // mount *should* be implement. This is just a nicer message than a Page Fault
    if(!rootfs->mount)
        kpanic("rootfs does not support mounting");
    if((e=rootfs->mount(rootfs, &kernel.rootBlock, NULL)) < 0)
        kpanic("Failed to mount rootfs: %s", status_str(e));
}
intptr_t inode_creat(Inode* parent, const char* name, size_t namelen, oflags_t flags) {
    if(!parent->ops->creat) return -UNSUPPORTED;
    return parent->ops->creat(parent, name, namelen, flags);
}
intptr_t inode_get_dir_entries(Inode* dir, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes) {
    if(!dir->ops->get_dir_entries) return -UNSUPPORTED;
    return dir->ops->get_dir_entries(dir, entries, size, offset, read_bytes);
}
intptr_t inode_find(Inode* dir, const char* name, size_t namelen, Inode** inode) {
    if(!dir->ops->find) return -UNSUPPORTED;
    intptr_t e;
    inodeid_t id;
    if((e=dir->ops->find(dir, name, namelen, &id)) < 0) return e;
    return fetch_inode(dir->superblock, id, inode);
}
intptr_t inode_read(Inode* file, void* buf, size_t size, off_t offset) {
    if(!file->ops->read) return -UNSUPPORTED;
    return file->ops->read(file, buf, size, offset);
}
intptr_t inode_write(Inode* file, const void* buf, size_t size, off_t offset) {
    if(!file->ops->write) return -UNSUPPORTED;
    return file->ops->write(file, buf, size, offset);
}
intptr_t inode_ioctl(Inode* file, Iop op, void* arg) {
    if(!file->ops->ioctl) return -UNSUPPORTED;
    return file->ops->ioctl(file, op, arg);
}
intptr_t inode_mmap(Inode* file, MmapContext* context, void** addr, size_t size_pages) {
    if(!file->ops->mmap) return -UNSUPPORTED;
    return file->ops->mmap(file, context, addr, size_pages);
}
intptr_t inode_truncate(Inode* file, size_t size) {
    if(!file->ops->truncate) return -UNSUPPORTED;
    return file->ops->truncate(file, size);
}
intptr_t inode_stat(Inode* inode, Stats* stats) {
    if(!inode->ops->stat) return -UNSUPPORTED;
    return inode->ops->stat(inode, stats);
}
void     inode_cleanup(Inode* inode) {
    if(inode->ops->cleanup) inode->ops->cleanup(inode);
}

static const char* path_dir_next(const char* path) {
    while(*path) {
        if(*path++ == '/') {
            return path;
        }
    }
    return NULL;
}
intptr_t vfs_find_parent(Path* path, const char** pathend, Inode** inode) {
    intptr_t e = 0;
    const char* dirbegin = path->path;
    const char* dirend; 
    *inode = iget(path->from);
    while((dirend = path_dir_next(dirbegin))) {
        Inode* parent = *inode;
        if((e=inode_find(parent, dirbegin, dirend-1-dirbegin, inode)) < 0) {
            idrop(parent);
            return e;
        }
        idrop(parent);
        dirbegin = dirend;
    }
    *pathend = dirbegin;
    return 0;
}
intptr_t vfs_find(Path* path, Inode** inode) {
    intptr_t e;
    const char* pathend;
    Inode* parent;
    if((e=vfs_find_parent(path, &pathend, &parent)) < 0) return e;
    if(pathend[0] == '\0') { // <-- Its root
        *inode = parent;
        return 0;
    } 
    if((e=inode_find(parent, pathend, strlen(pathend), inode)) < 0) {
        idrop(parent);
        return e;
    }
    idrop(parent);
    return 0;
}
intptr_t vfs_creat(Path* path, oflags_t flags) {
    Inode* parent;
    intptr_t e;
    const char* pathend;
    if((e=vfs_find_parent(path, &pathend, &parent)) < 0) return e;
    if(pathend[0] == '\0') {
        idrop(parent);
        return -ALREADY_EXISTS;
    }
    if((e=inode_creat(parent, pathend, strlen(pathend), flags)) < 0) {
        idrop(parent);
        return e;
    }
    idrop(parent);
    return 0;
}
intptr_t vfs_register_device(const char* name, Device* device) {
    intptr_t e;
    Inode* devices;
    if((e=vfs_find_abs("/devices", &devices)) < 0) return e;
    if(devices->superblock->fs != &tmpfs) kpanic("/devices is not inside tmpfs");
    e = tmpfs_register_device(devices, device, name, strlen(name));
    idrop(devices);
    return e;
}
intptr_t parse_abs(const char* path, Path* res) {
    if(path[0] != '/') return -INVALID_PATH;
    path++;
    intptr_t e;
    Inode* root;
    if((e=fetch_inode(&kernel.rootBlock, kernel.rootBlock.root, &root)) < 0) return e;
    res->from = root; 
    res->path = path;
    return 0;
}
