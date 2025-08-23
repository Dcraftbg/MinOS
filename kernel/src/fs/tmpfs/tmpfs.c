#include "tmpfs.h"
#include <vfs.h>
#include <utils.h>
#include <mem/slab.h> 
#include <log.h>
typedef struct {
    Inode inode;
    char name[TMPFS_NAME_LIMIT];
    // NOTE: can be both Device* and TmpfsData* depending on the kind
    void* data;
} TmpfsInode;
typedef struct TmpfsData TmpfsData;
struct TmpfsData {
    TmpfsData* next;
    char data[];
};
#define TMPFS_DATABLOCK_BYTES (PAGE_SIZE-sizeof(TmpfsData))
#define TMPFS_DATABLOCK_DENTS ((TMPFS_DATABLOCK_BYTES) / sizeof(TmpfsInode*))
static TmpfsData* datablock_new() {
    TmpfsData* data = (TmpfsData*)kernel_malloc(PAGE_SIZE);
    if(!data) return NULL;
    data->next = NULL;
    memset(data->data, 0, PAGE_SIZE-sizeof(*data));
    return data;
}
static void datablock_destroy(TmpfsData* data) {
    kernel_dealloc(data, PAGE_SIZE);
}
static Cache* tmpfs_inode_cache    = NULL;

static TmpfsInode* tmpfs_new_inode(Superblock* sb, size_t size, uint8_t kind, void* data, const char* name, size_t namelen);
// NOTE: DOES NOT CLEANUP SUBENTRIES FOR DIRECTORIES.
static void tmpfs_inode_destroy(TmpfsInode* inode) {
    TmpfsData* head = inode->data;
    while(head) {
        TmpfsData* next = head->next;
        datablock_destroy(head);
        head = next;
    }
    cache_dealloc(tmpfs_inode_cache, inode);
}
static TmpfsInode* directory_new(Superblock* sb, const char* name, size_t namelen) {
    return tmpfs_new_inode(sb, 0, STX_TYPE_DIR, NULL, name, namelen);
}
static TmpfsInode* file_new(Superblock* sb, const char* name, size_t namelen) {
    return tmpfs_new_inode(sb, 0, STX_TYPE_FILE, NULL, name, namelen);
}
static TmpfsInode* device_new(Superblock* sb, Inode* device, const char* name, size_t namelen) {
    return tmpfs_new_inode(sb, 0, STX_TYPE_DEVICE, device, name, namelen);
}
static TmpfsInode* socket_new(Superblock* sb, Inode* sock, const char* name, size_t namelen) {
    return tmpfs_new_inode(sb, 0, STX_TYPE_MINOS_SOCKET, sock, name, namelen);
}
static intptr_t tmpfs_put(TmpfsInode* dir, TmpfsInode* entry) {
    TmpfsData* prev = (TmpfsData*)(&(dir->data));
    TmpfsData* head = dir->data;
    size_t size = dir->inode.size;
    while(head && size >= TMPFS_DATABLOCK_DENTS) {
        size -= TMPFS_DATABLOCK_DENTS;
        prev = head;
        head = head->next;
    }
    if(size >= TMPFS_DATABLOCK_DENTS) return -FILE_CORRUPTION;
    if(!head) {
        TmpfsData* new_block = datablock_new();
        if(!new_block) return -NOT_ENOUGH_MEM;
        prev->next = new_block;
        head = new_block;
    }
    ((TmpfsInode**)head->data)[size] = entry;
    dir->inode.size++;
    return 0;
}
intptr_t tmpfs_socket_creat(Inode* parent, Inode* sock, const char* name, size_t namelen) {
    if(parent->type != STX_TYPE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode = socket_new(parent->superblock, sock, name, namelen);
    if(!inode) return -NOT_ENOUGH_MEM;
    intptr_t e;
    if((e=tmpfs_put((TmpfsInode*)parent, inode)) < 0) {
        tmpfs_inode_destroy(inode);
        return e;
    }
    return 0;
}
// TODO: Check whether entry already exists or not
static intptr_t tmpfs_creat(Inode* parent, const char* name, size_t namelen, oflags_t flags, Inode** result) {
    if(parent->type != STX_TYPE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode;
    if(flags & O_DIRECTORY) inode=directory_new(parent->superblock, name, namelen);
    else inode=file_new(parent->superblock, name, namelen);
    if(!inode) return -NOT_ENOUGH_MEM;
    intptr_t e;
    if((e=tmpfs_put((TmpfsInode*)parent, inode)) < 0) {
        tmpfs_inode_destroy(inode);
        return e;
    }
    *result = iget(&inode->inode);
    return 0;
}
static intptr_t tmpfs_get_dir_entries(Inode* dir, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes) { 
    if(dir->type != STX_TYPE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode = (TmpfsInode*)dir;
    if(offset > inode->inode.size) return -INVALID_OFFSET;
    if(offset == inode->inode.size) {
        *read_bytes = 0;
        return 0;
    }
    TmpfsData* head = inode->data;
    while(head && (size_t)offset >= TMPFS_DATABLOCK_DENTS) {
        offset -= TMPFS_DATABLOCK_DENTS;
        head = head->next;
    }
    if(offset >= TMPFS_DATABLOCK_DENTS) return -FILE_CORRUPTION;
    const size_t left = inode->inode.size-offset;
    size_t read = 0;
    size_t rc   = 0;
    while(head && read+sizeof(DirEntry) < size && rc < left) {
        size_t to_read = left-rc;
        if(to_read > TMPFS_DATABLOCK_DENTS) to_read = TMPFS_DATABLOCK_DENTS;
        for(size_t i = offset; i < to_read; ++i) {
            DirEntry* entry = (DirEntry*)(((uint8_t*)entries)+read);
            TmpfsInode* tmpfs_inode = ((TmpfsInode**)head->data)[rc];
            size_t requires = sizeof(DirEntry)+strlen(tmpfs_inode->name)+1;
            if(read+requires > size) {
                *read_bytes = read;
                return rc;
            }
            entry->size    = requires;
            entry->inodeid = (ino_t)tmpfs_inode;
            entry->kind    = tmpfs_inode->inode.type;
            memcpy(entry->name, tmpfs_inode->name, strlen(tmpfs_inode->name)+1);
            read += requires;
            rc++;
        }
        head = head->next;
        offset = 0;
    }
    *read_bytes = read;
    return rc;
}
static intptr_t tmpfs_read(Inode* file, void* buf, size_t size, off_t offset) {
    if(!buf) return -INVALID_PARAM;
    if(file->type != STX_TYPE_FILE) return -INODE_IS_DIRECTORY;
    TmpfsInode* inode = (TmpfsInode*)file;
    if(offset > inode->inode.size) return -INVALID_OFFSET;
    if(offset == inode->inode.size || size == 0) return 0; // We couldn't read anything
    size_t left = inode->inode.size-offset;
    size = size < left ? size : left; // size = min(size,left);
    TmpfsData* block = inode->data;
    while(offset >= TMPFS_DATABLOCK_BYTES && block) {
        offset-=TMPFS_DATABLOCK_BYTES;
        block = block->next;
    }
    if(offset >= TMPFS_DATABLOCK_BYTES) return -FILE_CORRUPTION;
    size_t ressize = 0; // Count bytes read, in case our block list ends prematurely
    while(block && size) {
        // min(size, TMPFS_DATABLOCK_BYTES);
        size_t read_bytes = size < (TMPFS_DATABLOCK_BYTES-offset) ? size : (TMPFS_DATABLOCK_BYTES-offset);
        memcpy(buf, block->data+offset, read_bytes);
        size-=read_bytes;
        buf = (uint8_t*)buf+read_bytes;
        ressize+=read_bytes;
        block = block->next;
        offset = 0;
    }
    return ressize;
}
static intptr_t tmpfs_write(Inode* file, const void* buf, size_t size, off_t offset) {
    TmpfsInode* inode = (TmpfsInode*)file;
    if(inode->inode.type != STX_TYPE_FILE) return -INODE_IS_DIRECTORY;
    if((size_t)offset > inode->inode.size) return -INVALID_OFFSET;
    TmpfsData *head = inode->data, *prev=(TmpfsData*)(&inode->data);
    size_t left = inode->inode.size;
    while(head && (size_t)offset >= TMPFS_DATABLOCK_BYTES) {
        offset -= TMPFS_DATABLOCK_BYTES;
        left -= TMPFS_DATABLOCK_BYTES;
        prev = head;
        head = head->next;
    }
    if(!head && offset > 0) return -FILE_CORRUPTION;
    size_t written = 0;
    const uint8_t* bytes = buf;
    while(written < size) {
        size_t block_left = TMPFS_DATABLOCK_BYTES-offset;
        size_t to_write = size-written;
        if(to_write > block_left) to_write = block_left;
        if(!head) {
            prev->next = datablock_new();
            if(!prev->next) return written;
            head = prev->next;
        }
        if(to_write > left-offset) inode->inode.size += to_write-(left-offset);
        memcpy(head->data+offset, bytes+written, to_write);
        written += to_write;
        prev = head;
        head = head->next;
        offset = 0;
        if(left < TMPFS_DATABLOCK_BYTES) left=0;
        else left-=TMPFS_DATABLOCK_BYTES;
    }
    return written;
}
static intptr_t tmpfs_truncate(Inode* file, size_t size) {
    TmpfsInode* inode = (TmpfsInode*)file;
    if(inode->inode.type != STX_TYPE_FILE) return -INODE_IS_DIRECTORY;
    if(inode->inode.size == size) return 0;
    if(inode->inode.size < size) {
        TmpfsData *head = inode->data, *prev = (TmpfsData*)&inode->data;
        size_t n = 0;
        while(head && n < inode->inode.size) {
            n += TMPFS_DATABLOCK_BYTES;
            prev = head;
            head = head->next;
        }
        if(n < inode->inode.size) {
            kerror("File corruption. n=%zu inode->inode.size=%zu", n, inode->inode.size);
            return -FILE_CORRUPTION;
        }
        TmpfsData* start = head = prev;
        n -= TMPFS_DATABLOCK_BYTES;
        while(n < size) {
            n += TMPFS_DATABLOCK_BYTES;
            head->next = datablock_new();
            if(!head->next) {
                size = inode->inode.size;
                // Skip the first one that already existed
                start = start->next;
                // Cleanup the newly allocated blocks
                while(start) {
                    TmpfsData* next = start->next;
                    datablock_destroy(start);
                    start = next;
                }
                return -NOT_ENOUGH_MEM;
            }
            head = head->next;
        }
        inode->inode.size = size;
        return 0;
    }
    inode->inode.size = size;
    TmpfsData *data = inode->data, *prev = (TmpfsData*)&inode->data;
    size_t n = 0;
    while(data && n < size) {
        n += TMPFS_DATABLOCK_BYTES;
        prev = data;
        data = data->next;
    }
    if(!data) return -FILE_CORRUPTION;
    prev->next = NULL;
    while(data) {
        TmpfsData* next = data->next;
        datablock_destroy(data);
        data = next;
    }
    return 0;
}
static intptr_t tmpfs_find(Inode* dir, const char* name, size_t namelen, Inode** result) {
    TmpfsInode* inode = (TmpfsInode*)dir;
    if(inode->inode.type != STX_TYPE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsData* head = inode->data;
    size_t size = inode->inode.size;
    while(size && head) {
        size_t to_read = size > TMPFS_DATABLOCK_DENTS ? TMPFS_DATABLOCK_DENTS : size;
        for(size_t i = 0; i < to_read; ++i) {
            TmpfsInode* entry = ((TmpfsInode**)head->data)[i];
            if(strlen(entry->name) == namelen && memcmp(entry->name, name, namelen) == 0) {
                *result = entry->inode.type == STX_TYPE_DEVICE || entry->inode.type == STX_TYPE_MINOS_SOCKET ? iget(entry->data) : iget(&entry->inode);
                return 0;
            }
        }
        head = head->next;
        size -= to_read;
    }
    return size > 0 ? -FILE_CORRUPTION : -NOT_FOUND;
}
static InodeOps tmpfs_inode_ops = {
    .creat = tmpfs_creat,
    .get_dir_entries = tmpfs_get_dir_entries,
    .find  = tmpfs_find,
    .read  = tmpfs_read,
    .write = tmpfs_write,
    .truncate = tmpfs_truncate,
};

// NOTE: assumes namelen is in range
static TmpfsInode* tmpfs_new_inode(Superblock* sb, size_t size, uint8_t kind, void* data, const char* name, size_t namelen) {
    TmpfsInode* me = cache_alloc(tmpfs_inode_cache);
    if(!me) return NULL;
    inode_init(&me->inode, tmpfs_inode_cache);
    me->inode.superblock = sb;
    me->inode.type = kind;
    me->inode.ops = &tmpfs_inode_ops;
    me->inode.size = size;
    me->data = data;
    memcpy(me->name, name, namelen);
    me->name[namelen] = '\0';
    return me;
}

static intptr_t tmpfs_get_inode(Superblock* sb, ino_t id, Inode** result) {
    TmpfsInode* tmp_inode = (TmpfsInode*)id;
    *result = tmp_inode->inode.type == STX_TYPE_MINOS_SOCKET || tmp_inode->inode.type == STX_TYPE_DEVICE 
               ? (Inode*)tmp_inode->data : iget(&tmp_inode->inode);
    return 0;
}
// TODO: Unmount that unlinks and destroys everything.
static SuperblockOps tmpfs_superblock_ops = {
    .get_inode = tmpfs_get_inode,
};

static intptr_t tmpfs_init(Fs* _fs) {
    (void)_fs;
    if(!(tmpfs_inode_cache = create_new_cache(sizeof(TmpfsInode), "TmpfsInode"))) 
        return -NOT_ENOUGH_MEM;
    return 0;
}
static intptr_t tmpfs_deinit(Fs* _fs) {
    (void)_fs;
    return 0;
}
static intptr_t tmpfs_mount(Fs* fs, Superblock* superblock, Inode* on) {
    if(on) kwarn("tmpfs: File `%p` provided in mount for tmpfs in unused", on);
    TmpfsInode* root = directory_new(superblock, NULL, 0);
    superblock->fs = fs;
    superblock->root = (ino_t)root;
    superblock->ops  = &tmpfs_superblock_ops;
    return 0;
}
Fs tmpfs = {
    .priv   = NULL,
    .init   = tmpfs_init,
    .deinit = tmpfs_deinit,
    .mount  = tmpfs_mount
};
intptr_t tmpfs_register_device(Inode* parent, Inode* device, const char* name, size_t namelen) {
    if(parent->type != STX_TYPE_DIR) return -IS_NOT_DIRECTORY;
    device->type = STX_TYPE_DEVICE;
    TmpfsInode* inode=device_new(parent->superblock, device, name, namelen);
    if(!inode) return -NOT_ENOUGH_MEM;
    intptr_t e;
    if((e=tmpfs_put((TmpfsInode*)parent, inode)) < 0) {
        tmpfs_inode_destroy(inode);
        return e;
    }
    return 0;
}
