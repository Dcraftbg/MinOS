#pragma once
#include <stdint.h>
#include <stddef.h>
#include <minos/status.h>
#include <minos/fsdefs.h>
#include <collections/list.h>
#include "utils.h"
#include "page.h"
typedef struct {
    page_t page_table;
    struct list* memlist;
} MmapContext;
typedef struct FsOps FsOps;
typedef struct InodeOps InodeOps;
typedef struct Inode Inode;
typedef struct VfsDir VfsDir;
typedef struct Superblock Superblock;
typedef struct {
    Inode* inode;
    FsOps* ops;
    off_t cursor;
    void* private; // FS defined
    // FIXME: Remove this
    fmode_t mode;
} VfsFile;
#define vfsfile_constr(file, v_inode, v_ops, v_cursor, v_private, v_mode) \
   do {\
       (file)->inode = (v_inode);\
       (file)->ops = (v_ops);\
       (file)->cursor = (v_cursor);\
       (file)->private = (v_private);\
       (file)->mode = (v_mode);\
   } while(0)
typedef struct VfsDir {
    Inode* inode;
    FsOps* ops;
    void* private;
    off_t cursor;
} VfsDir;
typedef struct Inode {
    Superblock* superblock;
    _Atomic size_t shared;
    fmode_t mode;
    InodeOps* ops;
    inodekind_t kind;
    inodeid_t inodeid;
    void* private;
} Inode;
typedef struct VfsDirEntry {
    Superblock* superblock;
    FsOps* ops;
    inodekind_t kind;
    inodeid_t inodeid;
    void* private;
} VfsDirEntry;
#define vfsdirentry_constr(entry, v_superblock, v_ops, v_kind, v_inodeid, v_private) \
    do {\
        (entry)->superblock = (v_superblock);\
        (entry)->ops        = (v_ops);\
        (entry)->kind       = (v_kind);\
        (entry)->inodeid    = (v_inodeid);\
        (entry)->private    = (v_private);\
    } while(0)
typedef uint32_t Iop;
struct FsOps {
    // Ops for directories
    intptr_t (*create)(VfsDir* parent, VfsDirEntry* result);         // @check ops
    intptr_t (*mkdir)(VfsDir* parent, VfsDirEntry* result);          // @check ops
    intptr_t (*get_dir_entries)(VfsDir* dir, VfsDirEntry* entries, size_t count, off_t offset); // @check ops

    // Ops for direntries
    intptr_t (*identify)(VfsDirEntry* this, char* namebuf, size_t namecap);
    intptr_t (*rename)(VfsDirEntry* this, const char* name, size_t namelen);

    // Ops for files
    intptr_t (*read)(VfsFile* file, void* buf, size_t size, off_t offset);
    intptr_t (*write)(VfsFile* file, const void* buf, size_t size, off_t offset);
    intptr_t (*seek)(VfsFile* file, off_t offset, seekfrom_t from); 
    intptr_t (*ioctl)(VfsFile* file, Iop op, void* arg);
    intptr_t (*mmap)(VfsFile* file, MmapContext* context, void** addr, size_t size_pages);

    // Close
    void (*close)(VfsFile* file);
    void (*dirclose)(VfsDir* dir);
};


#ifdef INODEMAP_DEFINE
#define HASHMAP_DEFINE
#endif
#include "mem/slab.h"
#include "string.h"
#include "memory.h"
#include <collections/hashmap.h>
#define INODEMAP_EQ(A,B) A==B
#define INODEMAP_HASH(K) K
#define INODEMAP_ALLOC kernel_malloc
#define INODEMAP_DEALLOC kernel_dealloc
#define INODEMAP_PAIR_ALLOC(_) cache_alloc(hashpair_cache) 
#define INODEMAP_PAIR_DEALLOC(ptr, size) cache_dealloc(hashpair_cache, ptr)
MAKE_HASHMAP_EX2(InodeMap, inodemap, Inode*, inodeid_t, INODEMAP_HASH, INODEMAP_EQ, INODEMAP_ALLOC, INODEMAP_DEALLOC, INODEMAP_PAIR_ALLOC, INODEMAP_PAIR_DEALLOC)

#ifdef INODEMAP_DEFINE
#undef HASHMAP_DEFINE
#endif


#include <sync/rwlock.h>
struct Superblock {
    inodeid_t root;
    InodeMap inodemap;
    Mutex inodemap_lock;
    intptr_t (*get_inode)(struct Superblock* sb, inodeid_t id, Inode** result);
};
/*
typedef struct {
    struct list next;
    Inode* inode;
    Superblock* superblock;
} Mount;
*/
typedef struct Device Device;
struct Device {
    InodeOps* ops;
    void* private;
    intptr_t (*init_inode)(Device* device, Inode* inode);
};
struct InodeOps {
    intptr_t (*stat)(Inode* this, Stats* stats);
    // NOTE: mode is only used for permission checks by the driver
    intptr_t (*open)(Inode* this, VfsFile* result, fmode_t mode);    // @check ops
    // NOTE: mode is only used for permission checks by the driver
    intptr_t (*diropen)(Inode* this, VfsDir* result, fmode_t mode);  // @check ops
    intptr_t (*close)(Inode* this, VfsFile* result);                 // @check ops
    // Only called when shared=0 to cleanup memory
    void (*cleanup)(Inode* this); 
    // TODO: unlink which will free all memory of that inode. But only the inode itself, not its children (job of caller (vfs))
};
typedef struct {
    FsOps *fs_ops;
    InodeOps *inode_ops;
    intptr_t (*init)(Superblock* superblock);
    intptr_t (*deinit)(Superblock* superblock);
} FsDriver;

typedef struct {
    struct {
        Superblock* superblock;
        inodeid_t id;
    } from;
    const char* path;
} Path;


void init_vfs();


// Return value:
// >=0 Success
// < 0 Error 
intptr_t vfs_find_parent(Path* path, const char** pathend, Superblock** sb, inodeid_t* id);

// Return value:
//   0 Success
// < 0 Error 
intptr_t vfs_find(Path* path, Superblock** sb, inodeid_t* id);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_create(Path* path);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_open(Path* path, VfsFile* result, fmode_t mode);


// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_mkdir(Path* path);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_diropen(Path* path, VfsDir* result, fmode_t mode);

// Return value:
// >=0 Amount of bytes written
// < 0 Error
intptr_t vfs_write(VfsFile* result, const void* buf, size_t size);

// Return value:
// >=0 Amount of bytes read
// < 0 Error
intptr_t vfs_read(VfsFile* result, void* buf, size_t size);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_close(VfsFile* result);

// Return value:
// >=0 Number of entries read
// < 0 Error
intptr_t vfs_get_dir_entries(VfsDir* dir, VfsDirEntry* entries, size_t count);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_dirclose(VfsDir* result);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_identify(VfsDirEntry* entry, char* namebuf, size_t namecap);

// NOTE: 
//   0 Success
// < 0 Error
intptr_t fetch_inode(Superblock* sb, inodeid_t id, Inode** result, fmode_t mode);

// Return value:
// >= 0 Where the cursor should be
// <  0 Error
intptr_t vfs_seek(VfsFile* file, off_t offset, seekfrom_t from);

// Return value:
// >= 0 Success
// <  0 Error
intptr_t vfs_stat(Inode* this, Stats* stats);

// Return value:
// >= 0 Success
// <  0 Error
intptr_t vfs_stat_entry(VfsDirEntry* this, Stats* stats);

// Return value:
// >= 0 Success (Implementation defined value)
// <  0 Error
intptr_t vfs_ioctl(VfsFile* file, Iop op, void* arg);

// Return value:
// >= 0 Success (Implementation defined value)
// <  0 Error
intptr_t vfs_mmap(VfsFile* file, MmapContext* context, void** addr, size_t size_pages);

intptr_t vfs_register_device(const char* name, Device* device);
#define MAX_INODE_NAME 128


// TODO: Figure out a way to inline these 
// NOTE: Functions for fs drivers
// Inode* vfs_alloc_inode(Superblock* superblock);
Inode* vfs_new_inode();
Inode* iget(Inode* inode);
#if 0
#define idrop(inode) _idrop(__func__, inode)
void _idrop(const char* func, Inode* inode);
#else
void idrop(Inode* inode);
#endif

#define PATH_ABS(p) { .from = {.superblock=&kernel.rootBlock, .id=kernel.rootBlock.root }, .path=p }

intptr_t parse_abs(const char* path, Path* res);
static intptr_t vfs_find_parent_abs(const char* path, const char** pathend, Superblock** sb, inodeid_t* id) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_find_parent(&abs, pathend, sb, id);
}

static intptr_t vfs_find_abs(const char* path, Superblock** sb, inodeid_t* id) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_find(&abs, sb, id);
}

static intptr_t vfs_create_abs(const char* path) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_create(&abs);
}

static intptr_t vfs_open_abs(const char* path, VfsFile* result, fmode_t mode) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_open(&abs, result, mode);
}

static intptr_t vfs_mkdir_abs(const char* path) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_mkdir(&abs);
}

static intptr_t vfs_diropen_abs(const char* path, VfsDir* result, fmode_t mode) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_diropen(&abs, result, mode);
}

