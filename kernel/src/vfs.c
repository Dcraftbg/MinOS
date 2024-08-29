#define INODEMAP_DEFINE
#include <stddef.h>
struct Cache* hashpair_cache = NULL;
#include "kernel.h"
#include "vfs.h"
#include "./fs/tmpfs/tmpfs.h"
#include "slab.h"
#include "string.h"
#include "debug.h"

// TODO: Introduce a setup driver function that will set all missing functions to a function that just returns -UNSUPPORTED; And make helper functions just call that instead of making a check

static void _vfs_cleanup(Inode* inode);

static void _log_inode(const char* fname, Inode* inode);

Inode* vfs_new_inode() {
    Inode* inode = cache_alloc(kernel.inode_cache);
    if(inode) memset(inode, 0, sizeof(*inode));
    return inode;
}
Inode* iget(Inode* inode) {
    inode->shared++;
    return inode;
}
#if 0
void _idrop(const char* func, Inode* inode) {
    debug_assert(inode->shared);
    if(inode->shared == 1) {
        // assert(inode->inodeid != kernel.rootBlock.rootEntry.inodeid);
        printf("[idrop] (%s) before removing: %zu elements\n",func, inode->superblock->inodemap.len);
        assert(inodemap_remove(&inode->superblock->inodemap, inode->inodeid));
        printf("[idrop] (%s) after removing: %zu elements\n",func, inode->superblock->inodemap.len);
        _vfs_cleanup(inode);
        cache_dealloc(kernel.inode_cache, inode);
        return;
    }
    inode->shared--;
}
#else
void idrop(Inode* inode) {
    debug_assert(inode->shared);
    if(inode->shared == 1) {
        assert(inodemap_remove(&inode->superblock->inodemap, inode->inodeid));
        _vfs_cleanup(inode);
        cache_dealloc(kernel.inode_cache, inode);
        return;
    }
    inode->shared--;
}
#endif
static void _vfs_cleanup(Inode* inode) {
    if(inode->ops->cleanup) {
        inode->ops->cleanup(inode);
    }
}

static intptr_t _vfs_diropen(Inode* inode, VfsDir* result, fmode_t mode) {
    intptr_t e=0;
    if(!inode->ops->diropen) return -UNSUPPORTED;
    if((e=inode->ops->diropen(inode, result, mode)) < 0) return e;
    result->inode = iget(inode);
    return 0;
}
static intptr_t _vfs_dirclose(VfsDir* dir) {
    if(dir->ops->dirclose) dir->ops->dirclose(dir);
    idrop(dir->inode);
    return 0;
}
static intptr_t _vfs_open(Inode* inode, VfsFile* result, fmode_t mode) {
    intptr_t e=0;
    if(!inode->ops->open) return -UNSUPPORTED;
    if((e=inode->ops->open(inode, result, mode)) < 0) return e;
    result->inode = iget(inode);
    result->mode = mode;
    return 0;
}

static intptr_t _vfs_close(VfsFile* file) {
    if(file->ops->close) file->ops->close(file);
    idrop(file->inode);
    return 0;
}
// TODO: shared counter for directories
static intptr_t _vfs_diriter_open(VfsDir* dir, VfsDirIter* result) {
    if(!dir->ops->diriter_open) return -UNSUPPORTED;
    return dir->ops->diriter_open(dir, result);
}
static intptr_t _vfs_diriter_next(VfsDirIter* iter, VfsDirEntry* result) {
    if(!iter->ops->diriter_next) return -UNSUPPORTED;
    return iter->ops->diriter_next(iter, result);
}
// TODO: shared counter for directories
static intptr_t _vfs_diriter_close(VfsDirIter* iter) {
    if(!iter->ops->diriter_close) return -UNSUPPORTED;
    return iter->ops->diriter_close(iter);
}
static intptr_t _vfs_identify(VfsDirEntry* this, char* namebuf, size_t namecap) {
    if(!this->ops->identify) return -UNSUPPORTED;
    return this->ops->identify(this, namebuf, namecap);
}
static intptr_t _vfs_get_inode_of(VfsDirEntry* this, Inode** result) {
    if(!this->ops->get_inode_of) return -UNSUPPORTED;
    return this->ops->get_inode_of(this, result);
}

static intptr_t _vfs_mkdir(VfsDir* parent, VfsDirEntry* result) {
    if(!parent->ops->mkdir) return -UNSUPPORTED;
    return parent->ops->mkdir(parent, result);
}
static intptr_t _vfs_rename(VfsDirEntry* this, const char* name, size_t namelen) {
    if(!this->ops->rename) return -UNSUPPORTED;
    return this->ops->rename(this, name, namelen);
}
static intptr_t _vfs_create(VfsDir* parent, VfsDirEntry* result) {
    if(!parent->ops->create) return -UNSUPPORTED;
    return parent->ops->create(parent,result);
}
static intptr_t _vfs_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
    if(!file->ops->write) return -UNSUPPORTED;
    return file->ops->write(file,buf,size,offset);
}
static intptr_t _vfs_read(VfsFile* file, void* buf, size_t size, off_t offset) {
    if(!file->ops->read) return -UNSUPPORTED;
    return file->ops->read(file,buf,size,offset);
} 
static intptr_t _vfs_seek(VfsFile* file, off_t offset, seekfrom_t from) {
    if(!file->ops->seek) return -UNSUPPORTED;
    return file->ops->seek(file,offset,from);
}

static intptr_t _vfs_stat(VfsDirEntry* this, VfsStats* stats) {
    if(!this->ops->stat) return -UNSUPPORTED;
    return this->ops->stat(this, stats);
}

intptr_t vfs_stat(VfsDirEntry* this, VfsStats* stats) {
    return _vfs_stat(this, stats);
}

intptr_t fetch_inode(VfsDirEntry* entry, Inode** result, fmode_t mode) {
    debug_assert(entry);
    debug_assert(entry->superblock);
    intptr_t e;
    Inode** ref = NULL;
    if((ref=inodemap_get(&entry->superblock->inodemap, entry->inodeid))) {
        *result = *ref;
        if ((*result)->mode & MODE_WRITE /*&& !((*result)->mode & MODE_STREAM)*/) {
            return -RESOURCE_BUSY;
        }
        if ((*result)->mode & MODE_READ && !((*result)->mode & MODE_WRITE)) {
            if(mode & MODE_WRITE) return -RESOURCE_BUSY;
            iget(*result);
            return 0;
        }
        return -INVALID_PARAM;
    }
    if((e=_vfs_get_inode_of(entry, result)) < 0) return e;
    (*result)->mode = mode;
    if(!inodemap_insert(&entry->superblock->inodemap, entry->inodeid, *result)) return -NOT_ENOUGH_MEM;
    return 0;
}
void init_vfs() {
    assert(kernel.inode_cache = create_new_cache(sizeof(Inode), "Inode"));
    assert(hashpair_cache = create_new_cache(sizeof(Pair_InodeMap), "Pair_InodeMap"));
    intptr_t e=0;
    FsDriver* root_driver = &tmpfs_driver; 
    if(root_driver->init) {
        if((e=root_driver->init(&kernel.rootBlock)) < 0) {
            printf("ERROR: Could not initialise root fs (%ld)!\n",e);
            kabort();
        }
    }
    if(!root_driver->fs_ops->mkdir) {
        printf("ERROR: Rootfs driver does not support necessary features\n");
        kabort();
    }
    if((e=root_driver->fs_ops->mkdir(NULL, &kernel.rootBlock.rootEntry)) < 0) {
        printf("ERROR: Could not create root node (%ld)!\n",e);
        kabort();
    }
    if((e=_vfs_rename(&kernel.rootBlock.rootEntry, "/", 1)) < 0) {
        printf("ERROR: Could not rename root node (%ld)!\n",e);
        kabort();
    }
}
static const char* path_dir_next(const char* path) {
    while(*path) {
        if(*path++ == '/') {
            return path;
        }
    }
    return NULL;
}
static intptr_t _vfs_find_within(VfsDir* dir, char* namebuf, size_t namecap, const char* what, size_t whatlen, VfsDirEntry* result) {
    intptr_t e = 0;
    VfsDirIter iter = {0};
    if((e=_vfs_diriter_open(dir, &iter)) < 0) {
        return e;
    }
    while((e = _vfs_diriter_next(&iter, result)) == 0) {
        if((e = _vfs_identify(result, namebuf, namecap)) < 0) {
            _vfs_diriter_close(&iter);
            return e;
        }
        size_t namelen = strlen(namebuf);
        if(namelen == whatlen) {
            if(strncmp(namebuf, what, namelen) == 0) {
                 _vfs_diriter_close(&iter);
                 return 0;
            } 
        }
    }
    _vfs_diriter_close(&iter);
    return e;
}

intptr_t vfs_find_parent(const char* path, VfsDirEntry* result) {
    intptr_t e = 0;
    const char* dirbegin = path;
    const char* dirend   = path_dir_next(dirbegin); 
    char namebuf[MAX_INODE_NAME]; // Usually safe to allocate on the stack
    VfsDir dir = {0};
    *result = kernel.rootBlock.rootEntry;
    if(!dirend) {
        return -NOT_FOUND;
    }
    dirbegin = dirend;
    while((dirend = path_dir_next(dirbegin))) {
        Inode* curdir;
        if((e=fetch_inode(result, &curdir, MODE_READ)) < 0) {
            return e;
        }
        if((e=_vfs_diropen(curdir, &dir, MODE_READ)) < 0) {
            return e;
        }
        idrop(curdir);
        curdir = NULL;
        if((e=_vfs_find_within(&dir, namebuf, sizeof(namebuf), dirbegin, dirend-dirbegin-1, result))) {
            _vfs_dirclose(&dir);
            return e;
        }
        _vfs_dirclose(&dir);
        dirbegin = dirend;
    }
    return dirbegin-path;
}


intptr_t vfs_find(const char* path, VfsDirEntry* result) {
    VfsDirEntry parent = {0};
    Inode* parent_inode = NULL;
    VfsDir parentdir={0};
    intptr_t e = 0;
    char namebuf[MAX_INODE_NAME];
    if((e=vfs_find_parent(path, &parent)) < 0) {
        return e;
    }
    // TODO: Consider optimising this by passing result instead of &parent
    if(e == strlen(path)) {
        *result = parent;
        return 0;
    }
    const char* child = path+e;

    if((e=fetch_inode(&parent, &parent_inode, MODE_READ)) < 0) {
        return e;
    }
    if((e=_vfs_diropen(parent_inode, &parentdir, MODE_READ)) < 0) {
        idrop(parent_inode);
        return e;
    }
    idrop(parent_inode);
    if ((e=_vfs_find_within(&parentdir, namebuf, sizeof(namebuf), child, strlen(child), result)) < 0) {
        _vfs_dirclose(&parentdir);
        return e;
    }
    _vfs_dirclose(&parentdir);
    return 0;
}

intptr_t vfs_mkdir(const char* path) {
    VfsDirEntry parent = {0};
    Inode* parent_inode=NULL;
    VfsDir parentdir={0};
    intptr_t e = 0;
    char namebuf[MAX_INODE_NAME];
    if((e=vfs_find_parent(path, &parent)) < 0) {
        return e;
    }
    if(e == strlen(path)) { // We are talking about root
        return -ALREADY_EXISTS;
    }
    const char* child = path+e;
    if((e=fetch_inode(&parent, &parent_inode, MODE_WRITE | MODE_READ)) < 0) {
        return e;
    }
    if((e=_vfs_diropen(parent_inode, &parentdir, MODE_WRITE | MODE_READ)) < 0) return e;
    idrop(parent_inode);
    VfsDirEntry entry = {0};
    if ((e=_vfs_find_within(&parentdir, namebuf, sizeof(namebuf), child, strlen(child), &entry)) < 0) {
        if((e=_vfs_mkdir(&parentdir, &entry)) < 0) {
           _vfs_dirclose(&parentdir);
           return e;
        }
        // TODO: Unlink in case rename fails
        if((e=_vfs_rename(&entry, child, strlen(child))) < 0) {
           _vfs_dirclose(&parentdir);
           return e;
        }
        _vfs_dirclose(&parentdir);
        return 0;
    }
    _vfs_dirclose(&parentdir);
    return -ALREADY_EXISTS;
}

intptr_t vfs_create(const char* path) {
    VfsDirEntry parent = {0};
    Inode* parent_inode=NULL;
    VfsDir parentdir={0};
    intptr_t e = 0;
    char namebuf[MAX_INODE_NAME];
    if((e=vfs_find_parent(path, &parent)) < 0) {
        return e;
    }
    if(e == strlen(path)) {
        return -INODE_IS_DIRECTORY;
    }
    const char* child = path+e;
    if((e=fetch_inode(&parent, &parent_inode, MODE_WRITE | MODE_READ)) < 0) {
        return e;
    }
    if((e=_vfs_diropen(parent_inode, &parentdir, MODE_WRITE | MODE_READ)) < 0) return e;
    idrop(parent_inode);
    VfsDirEntry entry = {0};
    if ((e=_vfs_find_within(&parentdir, namebuf, sizeof(namebuf), child, strlen(child), &entry)) < 0) {
        if((e=_vfs_create(&parentdir, &entry)) < 0) {
           _vfs_dirclose(&parentdir);
           return e;
        }
        // TODO: Unlink in case rename fails
        if((e=_vfs_rename(&entry, child, strlen(child))) < 0) {
           _vfs_dirclose(&parentdir);
           return e;
        }
        _vfs_dirclose(&parentdir);
        return 0;
    }
    _vfs_dirclose(&parentdir);
    return -ALREADY_EXISTS;
}

intptr_t vfs_open(const char* path, VfsFile* result, fmode_t mode) {
    VfsDirEntry entry = {0};
    intptr_t e = 0;
    if((e = vfs_find(path, &entry)) < 0) {
        return e;
    }
    Inode* inode = NULL;
    if((e = fetch_inode(&entry, &inode, mode)) < 0) {
        return e;
    }
    if((e=_vfs_open(inode, result, mode)) < 0) {
        idrop(inode);
        return e;
    }
    idrop(inode);
    return 0;
}

intptr_t vfs_diropen(const char* path, VfsDir* result, fmode_t mode) {
    VfsDirEntry entry = {0};
    intptr_t e = 0;
    if((e = vfs_find(path, &entry)) < 0) {
        return e;
    }

    Inode* inode = NULL;
    if((e = fetch_inode(&entry, &inode, mode)) < 0) {
        return e;
    }
    if((e=_vfs_diropen(inode, result, mode)) < 0) {
        idrop(inode);
        return e;
    }
    idrop(inode); // from the inode itself
    return 0;
}


intptr_t vfs_write(VfsFile* result, const void* buf, size_t size) {
    if(result == NULL) return -INVALID_PARAM;
    intptr_t e = _vfs_write(result, buf, size, result->cursor);
    if(e > 0) result->cursor += e;
    return e;
}

intptr_t vfs_read(VfsFile* result, void* buf, size_t size) {
    if(result == NULL) return -INVALID_PARAM;
    intptr_t e = _vfs_read(result, buf, size, result->cursor);
    if(e > 0) result->cursor += e;
    return e;
}


intptr_t vfs_close(VfsFile* result) {
    return _vfs_close(result);
}


intptr_t vfs_dirclose(VfsDir* result) {
    return _vfs_dirclose(result);
}


intptr_t vfs_diriter_open(VfsDir* dir, VfsDirIter* result) {
    return _vfs_diriter_open(dir, result);
}


intptr_t vfs_diriter_next(VfsDirIter* iter, VfsDirEntry* result) {
    return _vfs_diriter_next(iter, result);
}



intptr_t vfs_diriter_close(VfsDirIter* result) {
    return _vfs_diriter_close(result);
}


intptr_t vfs_identify(VfsDirEntry* entry, char* namebuf, size_t namecap) {
    return _vfs_identify(entry, namebuf, namecap);
}

intptr_t vfs_seek(VfsFile* file, off_t offset, seekfrom_t from) {
    return _vfs_seek(file, offset, from);
}

intptr_t vfs_register_device(const char* name, Device* device) {
    intptr_t e = 0;
    VfsDir devices = {0};
    if((e=vfs_diropen("/devices", &devices, MODE_WRITE | MODE_READ)) < 0) {
        return e;
    }

    if(devices.ops != tmpfs_driver.fs_ops) {
        printf("ERROR: /devices is not inside ramfs");
        kabort();
    } // /devices is located outside of RAMFS
    char namebuf[MAX_INODE_NAME];
    VfsDirEntry entry = {0};
    if ((e=_vfs_find_within(&devices, namebuf, sizeof(namebuf), name, strlen(name), &entry)) < 0) {
        if(e != -NOT_FOUND) {
            _vfs_dirclose(&devices);
            return e;
        }
        if((e=tmpfs_register_device(&devices, device, &entry)) < 0) {
            _vfs_dirclose(&devices);
            return e;
        }
        // TODO: Unregister device
        // TODO: Unlink in case rename fails
        if((e = _vfs_rename(&entry, name, strlen(name))) < 0) {
            _vfs_dirclose(&devices);
            return e;
        }
        _vfs_dirclose(&devices);
        return 0;
    }
    _vfs_dirclose(&devices);
    return -ALREADY_EXISTS;
}
