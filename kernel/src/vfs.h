#pragma once
#include <stdint.h>
#include <stddef.h>
#include <minos/status.h>
#include <minos/fsdefs.h>
#include <minos/socket.h>
#include <list_head.h>
#include "utils.h"
#include "page.h"
typedef struct {
    page_t page_table;
    struct list_head* memlist;
} MmapContext;
typedef struct InodeOps InodeOps;
typedef struct Inode Inode;
typedef struct SuperblockOps SuperblockOps;
typedef struct Superblock Superblock;
typedef struct Fs Fs;
typedef struct Inode {
    struct list_head list;
    Superblock* superblock;
    _Atomic size_t shared;
    InodeOps* ops;
    // General stat fields
    uint8_t type;
    size_t size;
    ino_t id;
    void* priv;
    struct Cache* cache; // <- The Cache the object is in
} Inode;
typedef uint32_t Iop;

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
MAKE_HASHMAP_EX2(InodeMap, inodemap, Inode*, ino_t, INODEMAP_HASH, INODEMAP_EQ, INODEMAP_ALLOC, INODEMAP_DEALLOC, INODEMAP_PAIR_ALLOC, INODEMAP_PAIR_DEALLOC)

#ifdef INODEMAP_DEFINE
#undef HASHMAP_DEFINE
#endif


#include <sync/rwlock.h>
struct SuperblockOps {
    intptr_t (*get_inode)(Superblock* sb, ino_t id, Inode** result);
    intptr_t (*unmount)(Superblock* sb);
};
struct Superblock {
    Fs* fs;
    ino_t root;
    InodeMap inodemap;
    Mutex inodemap_lock;
    SuperblockOps* ops;
};
typedef struct Device Device;
struct statx;
struct InodeOps {
    // Ops for directories
    intptr_t (*creat)(Inode* parent, const char* name, size_t namelen, oflags_t oflag, Inode** result);
    intptr_t (*get_dir_entries)(Inode* dir, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes);
    intptr_t (*find)(Inode* dir, const char* name, size_t namelen, Inode** inode);
    // Ops for files
    intptr_t (*read) (Inode* file, void* buf, size_t size, off_t offset);
    intptr_t (*write)(Inode* file, const void* buf, size_t size, off_t offset);
    intptr_t (*ioctl)(Inode* file, Iop op, void* arg);
    intptr_t (*mmap)(Inode* file, MmapContext* context, void** addr, size_t size_pages);
    intptr_t (*truncate)(Inode* file, size_t size);
    // Sockets
    intptr_t (*accept)(Inode* sock, Inode* result, struct sockaddr* addr, size_t *addrlen);
    intptr_t (*bind)(Inode* sock, struct sockaddr* addr, size_t addrlen);
    intptr_t (*listen)(Inode* sock, size_t n);
    intptr_t (*connect)(Inode* sock, const struct sockaddr* addr, size_t addrlen);
    // General API
    void (*cleanup)(Inode* inode); 
    bool (*is_readable)(Inode* file);
    bool (*is_hungup)(Inode* file);
    bool (*is_writeable)(Inode* file);
    // TODO: unlink which will free all memory of that inode. But only the inode itself, not its children (job of caller (vfs))
};
// Wrappers for directories
intptr_t inode_creat(Inode* parent, const char* name, size_t namelen, oflags_t oflags, Inode** result);
intptr_t inode_get_dir_entries(Inode* dir, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes);
intptr_t inode_find(Inode* dir, const char* name, size_t namelen, Inode** inode);
// Wrappers for files
intptr_t inode_read(Inode* file, void* buf, size_t size, off_t offset);
intptr_t inode_write(Inode* file, const void* buf, size_t size, off_t offset);
intptr_t inode_ioctl(Inode* file, Iop op, void* arg);
intptr_t inode_mmap(Inode* file, MmapContext* context, void** addr, size_t size_pages);
intptr_t inode_truncate(Inode* file, size_t size);
// Wrappers for Sockets
intptr_t inode_accept(Inode* sock, Inode* result, struct sockaddr* addr, size_t *addrlen);
intptr_t inode_bind(Inode* sock, struct sockaddr* addr, size_t addrlen);
intptr_t inode_listen(Inode* sock, size_t n);
intptr_t inode_connect(Inode* sock, const struct sockaddr* addr, size_t addrlen);
// Wrappers for generic API
// By default returns true
bool inode_is_readable(Inode* file); 
// By default returns false 
bool inode_is_hungup(Inode* file);
// By default returns true
bool inode_is_writeable(Inode* file); 
void     inode_cleanup(Inode* inode);


struct Fs {
    void* priv;
    intptr_t (*init)(Fs* fs);
    intptr_t (*deinit)(Fs* fs);
    intptr_t (*mount)(Fs* fs, Superblock* superblock, Inode* on);
};

typedef struct {
    Inode* from;
    const char* path;
} Path;

void init_vfs();

intptr_t vfs_register_device(const char* name, Inode* device);
#define MAX_INODE_NAME 128


// TODO: Figure out a way to inline these 
// NOTE: Functions for fs drivers
// Inode* vfs_alloc_inode(Superblock* superblock);
Inode* new_inode();
// Initialise the inode with its default fields
void inode_init(Inode* inode, Cache* cache);
Inode* iget(Inode* inode);

// Internal function. Destroys inode but doesn't remove it from the superblock list
void idestroy(Inode* inode);
#if 0
#define idrop(inode) _idrop(__func__, inode)
void _idrop(const char* func, Inode* inode);
#else
void idrop(Inode* inode);
#endif

intptr_t parse_abs(const char* path, Path* res);
intptr_t vfs_find_parent(Path* path, const char** pathend, Inode** inode);
intptr_t vfs_find(Path* path, Inode** inode);
static intptr_t vfs_find_parent_abs(const char* path, const char** pathend, Inode** inode) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_find_parent(&abs, pathend, inode);
}

static intptr_t vfs_find_abs(const char* path, Inode** inode) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_find(&abs, inode);
}

intptr_t vfs_creat(Path* path, oflags_t oflags, Inode** result);
static intptr_t vfs_creat_abs(const char* path, oflags_t oflags, Inode** result) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_creat(&abs, oflags, result);
}

typedef struct Socket Socket;
intptr_t vfs_socket_create(Path* path, Inode* sock);
static intptr_t vfs_socket_create_abs(const char* path, Inode* sock) {
    Path abs;
    intptr_t e;
    if((e=parse_abs(path, &abs)) < 0) return e;
    return vfs_socket_create(&abs, sock);

}
intptr_t fetch_inode(Superblock* sb, ino_t id, Inode** result);
