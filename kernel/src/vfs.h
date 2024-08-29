#pragma once
#include <stdint.h>
#include <stddef.h>
#include <minos/status.h>
#include "list.h"
typedef intptr_t off_t;
typedef enum {
    INODE_DIR,
    INODE_FILE,
    INODE_DEVICE,
    INODE_COUNT,
} InodeKind;
typedef size_t inodeid_t;
typedef int inodekind_t;
enum {
    MODE_READ=BIT(1),
    MODE_WRITE=BIT(2),
    /*append?*/
};
typedef uint32_t fmode_t;
struct FsOps;
struct InodeOps;
struct Inode;
struct VfsDir;
struct Superblock;
typedef struct {
    struct VfsDir* dir;
    struct FsOps* ops;
    void* private; // (State) FS defined
} VfsDirIter;
typedef struct {
    struct Inode* inode;
    struct FsOps* ops;
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
    struct Inode* inode;
    struct FsOps* ops;
    void* private;
} VfsDir;
typedef struct {
    size_t lba ; // lba is in 1<<lba bytes
    union {
        size_t size; // In lba
        struct { uint32_t width; uint32_t height; };
    };
} VfsStats;
typedef struct Inode {
    struct Superblock* superblock;
    _Atomic size_t shared;
    fmode_t mode;
    struct InodeOps* ops;
    inodekind_t kind;
    inodeid_t inodeid;
    void* private;
} Inode;
typedef struct VfsDirEntry {
    struct Superblock* superblock;
    struct FsOps* ops;
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
enum {
    SEEK_START,
    SEEK_CURSOR,
    SEEK_END,
};
typedef uintptr_t seekfrom_t;
typedef struct FsOps {
    // Ops for directories
    intptr_t (*create)(VfsDir* parent, VfsDirEntry* result);         // @check ops
    intptr_t (*mkdir)(VfsDir* parent, VfsDirEntry* result);          // @check ops

    intptr_t (*diriter_open)(VfsDir* dir, VfsDirIter* result); // @check ops

    // Ops for diriters
    intptr_t (*diriter_next)(VfsDirIter* iter, VfsDirEntry* result); // @check ops
    intptr_t (*diriter_close)(VfsDirIter* iter);

    // Ops for direntries
    intptr_t (*identify)(VfsDirEntry* this, char* namebuf, size_t namecap);
    intptr_t (*stat)(VfsDirEntry* this, VfsStats* stats);
    intptr_t (*get_inode_of)(VfsDirEntry* this, Inode** result);
    intptr_t (*rename)(VfsDirEntry* this, const char* name, size_t namelen);


    // Ops for files
    intptr_t (*read)(VfsFile* file, void* buf, size_t size, off_t offset);
    intptr_t (*write)(VfsFile* file, const void* buf, size_t size, off_t offset);
    intptr_t (*seek)(VfsFile* file, off_t offset, seekfrom_t from); 

    // Close
    void (*close)(VfsFile* file);
    void (*dirclose)(VfsDir* dir);
} FsOps;


#ifdef INODEMAP_DEFINE
#define HASHMAP_DEFINE
#endif
#include "slab.h"
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



typedef struct Superblock {
    VfsDirEntry rootEntry;
    InodeMap inodemap;
} Superblock;
/*
typedef struct {
    struct list next;
    Inode* inode;
    Superblock* superblock;
} Mount;
*/
struct Device;
typedef struct Device {
    FsOps* ops;
    void* private;
    intptr_t (*open)(struct Device* this, VfsFile* file, fmode_t mode);
    intptr_t (*init)();
    intptr_t (*deinit)();
    intptr_t (*stat)(struct Device* this, VfsStats* stats);
} Device;
typedef struct InodeOps {
    // NOTE: mode is only used for permission checks by the driver
    intptr_t (*open)(Inode* this, VfsFile* result, fmode_t mode);    // @check ops
    // NOTE: mode is only used for permission checks by the driver
    intptr_t (*diropen)(Inode* this, VfsDir* result, fmode_t mode);  // @check ops
    intptr_t (*close)(Inode* this, VfsFile* result);                 // @check ops
    // Only called when shared=0 to cleanup memory
    void (*cleanup)(Inode* this); 
    // TODO: unlink which will free all memory of that inode. But only the inode itself, not its children (job of caller (vfs))
} InodeOps;
typedef struct {
    FsOps *fs_ops;
    InodeOps *inode_ops;
    intptr_t (*init)(Superblock* superblock);
    intptr_t (*deinit)(Superblock* superblock);
} FsDriver;



void init_vfs();
// Return value:
// >=0 Offset within the path at which child name begins
// < 0 Error 
intptr_t vfs_find_parent(const char* path, VfsDirEntry* result);

// Return value:
//   0 Success
// < 0 Error 
intptr_t vfs_find(const char* path, VfsDirEntry* result);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_create(const char* path);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_open(const char* path, VfsFile* result, fmode_t mode);


// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_mkdir(const char* path);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_diropen(const char* path, VfsDir* result, fmode_t mode);


// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_diriter_open(VfsDir* dir, VfsDirIter* result);

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
//   0 Success
// < 0 Error
intptr_t vfs_dirclose(VfsDir* result);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_diriter_next(VfsDirIter* iter, VfsDirEntry* result);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_diriter_close(VfsDirIter* result);


// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_identify(VfsDirEntry* entry, char* namebuf, size_t namecap);

// NOTE: 
//   0 Success
// < 0 Error
intptr_t fetch_inode(VfsDirEntry* entry, Inode** result, fmode_t mode);

// Return value:
// >= 0 Where the cursor should be
// <  0 Error
intptr_t vfs_seek(VfsFile* file, off_t offset, seekfrom_t from);


// Return value:
// >= 0 Success
// <  0 Error
intptr_t vfs_stat(VfsDirEntry* this, VfsStats* stats);

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


