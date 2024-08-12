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
    fmode_t mode;
} VfsFile;
typedef struct VfsDir {
    struct Inode* inode;
    struct FsOps* ops;
    void* private;
} VfsDir;
typedef struct Inode {
    // struct list list;
    _Atomic size_t shared;
    struct InodeOps* ops;
    size_t lba ; // lba is in 1<<lba bytes
    size_t size; // In lba
    inodekind_t kind;
    inodeid_t inodeid;
    void* private;
} Inode;
enum {
    SEEK_START,
    SEEK_CURSOR,
    SEEK_END,
};
typedef uintptr_t seekfrom_t;
typedef struct FsOps {
    // Ops for directories
    intptr_t (*create)(VfsDir* parent, Inode* result);         // @check ops
    intptr_t (*mkdir)(VfsDir* parent, Inode* result);          // @check ops
    // TODO: intptr_t (*create_device)(VfsDir* parent, FsDriver* driver, Inode* result);  // @check ops
    intptr_t (*diriter_open)(VfsDir* dir, VfsDirIter* result); // @check ops

    // Ops for diriters
    intptr_t (*diriter_next)(VfsDirIter* iter, Inode* result); // @check ops
    intptr_t (*diriter_close)(VfsDirIter* iter);

    // Ops for files
    intptr_t (*read)(VfsFile* file, void* buf, size_t size, off_t offset);
    intptr_t (*write)(VfsFile* file, const void* buf, size_t size, off_t offset);
    intptr_t (*seek)(VfsFile* file, off_t offset, seekfrom_t from); 

    // Close
    void (*close)(VfsFile* file);
    void (*dirclose)(VfsDir* dir);
} FsOps;
typedef struct {
    struct list inodes;
    Inode* root;
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
} Device;
typedef struct InodeOps {
    intptr_t (*open)(Inode* this, VfsFile* result, fmode_t mode);            // @check ops
    intptr_t (*diropen)(Inode* this, VfsDir* result);          // @check ops
    intptr_t (*rename)(Inode* this, const char* name, size_t namelen);
    intptr_t (*identify)(Inode* this, char* namebuf, size_t namecap);
    intptr_t (*close)(Inode* this, VfsFile* result);           // @check ops
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
intptr_t vfs_find_parent(const char* path, Inode** result);

// Return value:
//   0 Success
// < 0 Error 
intptr_t vfs_find(const char* path, Inode** result);

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
intptr_t vfs_diropen(const char* path, VfsDir* result);


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
intptr_t vfs_diriter_next(VfsDirIter* iter, Inode* result);

// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_diriter_close(VfsDirIter* result);


// Return value:
//   0 Success
// < 0 Error
intptr_t vfs_identify(Inode* inode, char* namebuf, size_t namecap);

// Return  value:
// >= 0 Where the cursor should be
// <  0 Error
intptr_t vfs_seek(VfsFile* file, off_t offset, seekfrom_t from);

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
#endif

void idrop(Inode* inode);
