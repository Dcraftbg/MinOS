#include "tmpfs.h"
#include "../../mem/slab.h" 
#include "../../log.h"
typedef struct {
    inodekind_t kind;
    char name[TMPFS_NAME_LIMIT];
    size_t size;
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
// NOTE: assumes namelen is in range
static TmpfsInode* tmpfs_new_inode(size_t size, inodekind_t kind, void* data, const char* name, size_t namelen) {
    TmpfsInode* me = cache_alloc(tmpfs_inode_cache);
    if(!me) return NULL;
    me->size = size;
    me->kind = kind;
    me->data = data;
    memcpy(me->name, name, namelen);
    me->name[namelen] = '\0';
    return me;
}
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
static TmpfsInode* directory_new(const char* name, size_t namelen) {
    return tmpfs_new_inode(0, INODE_DIR, NULL, name, namelen);
}
static TmpfsInode* file_new(const char* name, size_t namelen) {
    return tmpfs_new_inode(0, INODE_FILE, NULL, name, namelen);
}
static TmpfsInode* device_new(Device* device, const char* name, size_t namelen) {
    return tmpfs_new_inode(0, INODE_DEVICE, device, name, namelen);
}
static TmpfsInode* socket_new(Socket* sock, const char* name, size_t namelen) {
    return tmpfs_new_inode(0, INODE_MINOS_SOCKET, sock, name, namelen);
}
static intptr_t tmpfs_put(TmpfsInode* dir, TmpfsInode* entry) {
    TmpfsData* prev = (TmpfsData*)(&(dir->data));
    TmpfsData* head = dir->data;
    size_t size = dir->size;
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
    dir->size++;
    return 0;
}
intptr_t tmpfs_socket_creat(Inode* parent, Socket* sock, const char* name, size_t namelen) {
    if(parent->kind != INODE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode = socket_new(sock, name, namelen);
    if(!inode) return -NOT_ENOUGH_MEM;
    intptr_t e;
    if((e=tmpfs_put(parent->priv, inode)) < 0) {
        tmpfs_inode_destroy(inode);
        return e;
    }
    return 0;
}
Socket* tmpfs_get_socket(Inode* inode) {
    if(inode->kind != INODE_MINOS_SOCKET) return NULL;
    TmpfsInode* tmp_inode = inode->priv;
    return (Socket*)tmp_inode->data;
}
// TODO: Check whether entry already exists or not
static intptr_t tmpfs_creat(Inode* parent, const char* name, size_t namelen, oflags_t flags) {
    if(parent->kind != INODE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode;
    if(flags & O_DIRECTORY) inode=directory_new(name, namelen);
    else inode=file_new(name, namelen);
    if(!inode) return -NOT_ENOUGH_MEM;
    intptr_t e;
    if((e=tmpfs_put(parent->priv, inode)) < 0) {
        tmpfs_inode_destroy(inode);
        return e;
    }
    return 0;
}
static intptr_t tmpfs_get_dir_entries(Inode* dir, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes) { 
    if(dir->kind != INODE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode = dir->priv;
    if(offset > inode->size) return -INVALID_OFFSET;
    if(offset == inode->size) {
        *read_bytes = 0;
        return 0;
    }
    TmpfsData* head = inode->data;
    while(head && (size_t)offset >= TMPFS_DATABLOCK_DENTS) {
        offset -= TMPFS_DATABLOCK_DENTS;
        head = head->next;
    }
    if(offset >= TMPFS_DATABLOCK_DENTS) return -FILE_CORRUPTION;
    const size_t left = inode->size-offset;
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
            entry->inodeid = (inodeid_t)tmpfs_inode;
            entry->kind    = tmpfs_inode->kind;
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
    if(file->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    TmpfsInode* inode = (TmpfsInode*)file->priv;
    if(offset > inode->size) return -INVALID_OFFSET;
    if(offset == inode->size || size == 0) return 0; // We couldn't read anything
    size_t left = inode->size-offset;
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
    TmpfsInode* inode = file->priv;
    if(inode->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    if((size_t)offset > inode->size) return -INVALID_OFFSET;
    TmpfsData *head = inode->data, *prev=(TmpfsData*)(&inode->data);
    size_t left = inode->size;
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
        if(to_write > left-offset) inode->size += to_write-(left-offset);
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
    TmpfsInode* inode = file->priv;
    if(inode->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    if(inode->size == size) return 0;
    if(inode->size < size) {
        TmpfsData *head = inode->data, *prev = (TmpfsData*)&inode->data;
        size_t n = 0;
        while(head && n < inode->size) {
            n += TMPFS_DATABLOCK_BYTES;
            prev = head;
            head = head->next;
        }
        if(n < inode->size) {
            kerror("File corruption. n=%zu inode->size=%zu", n, inode->size);
            return -FILE_CORRUPTION;
        }
        TmpfsData* start = head = prev;
        n -= TMPFS_DATABLOCK_BYTES;
        while(n < size) {
            n += TMPFS_DATABLOCK_BYTES;
            head->next = datablock_new();
            if(!head->next) {
                size = inode->size;
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
        inode->size = size;
        return 0;
    } 
    inode->size = size;
    TmpfsData* data = inode->data;
    size_t n = 0;
    while(data && n < size) {
        n += TMPFS_DATABLOCK_BYTES;
        data = data->next;
    }
    if(!data) return -FILE_CORRUPTION;
    while(data) {
        TmpfsData* next = data->next;
        datablock_destroy(data);
        data = next;
    }
    return 0;
}
static intptr_t tmpfs_find(Inode* dir, const char* name, size_t namelen, inodeid_t* id) {
    TmpfsInode* inode = dir->priv;
    if(inode->kind != INODE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsData* head = inode->data;
    size_t size = inode->size;
    while(size && head) {
        size_t to_read = size > TMPFS_DATABLOCK_DENTS ? TMPFS_DATABLOCK_DENTS : size;
        for(size_t i = 0; i < to_read; ++i) {
            TmpfsInode* entry = ((TmpfsInode**)head->data)[i];
            if(strlen(entry->name) == namelen && memcmp(entry->name, name, namelen) == 0) {
                *id = (inodeid_t)entry;
                return 0;
            }
        }
        head = head->next;
        size -= to_read;
    }
    return size > 0 ? -FILE_CORRUPTION : -NOT_FOUND;
}
static intptr_t tmpfs_stat(Inode* me, Stats* stats) {
    if(!me || !me->priv) return -BAD_INODE;
    TmpfsInode* inode = (TmpfsInode*)me->priv;
    memset(stats, 0, sizeof(*stats));
    stats->kind = inode->kind;
    switch(inode->kind) {
    case INODE_DIR: {
        // Explicit declarations
        stats->lba = PAGE_SHIFT;
        stats->size = ((inode->size*sizeof(TmpfsInode*))+(PAGE_SIZE-1))/PAGE_SIZE;
        return 0;
    } break;
    case INODE_FILE: {
        // Explicit declarations
        stats->lba = 0;
        stats->size = inode->size;
        return 0;
    } break;
    }
    return -UNSUPPORTED;
}
static InodeOps tmpfs_inode_ops = {
    .creat = tmpfs_creat,
    .get_dir_entries = tmpfs_get_dir_entries,
    .find  = tmpfs_find,
    .read  = tmpfs_read,
    .write = tmpfs_write,
    .truncate = tmpfs_truncate,
    .stat  = tmpfs_stat,
};


static intptr_t tmpfs_get_inode(Superblock* sb, inodeid_t id, Inode** result) {
    intptr_t e;
    TmpfsInode* tmp_inode = (TmpfsInode*)id;
    Inode* inode;
    *result = (inode = new_inode());
    if(!inode) return -NOT_ENOUGH_MEM;
    inode->kind = tmp_inode->kind;
    inode->superblock = sb;
    inode->id   = id;
    if(tmp_inode->kind == INODE_DEVICE) {
        Device* device = tmp_inode->data;
        if((e=device->init_inode(device, inode)) < 0) {
            idrop(inode);
            return e;
        }
        return e;
    }
    inode->ops  = &tmpfs_inode_ops;
    inode->priv = tmp_inode;
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
    TmpfsInode* root = directory_new(NULL, 0);
    superblock->fs = fs;
    superblock->root = (inodeid_t)root;
    superblock->ops  = &tmpfs_superblock_ops;
    return 0;
}
Fs tmpfs = {
    .priv   = NULL,
    .init   = tmpfs_init,
    .deinit = tmpfs_deinit,
    .mount  = tmpfs_mount
};
intptr_t tmpfs_register_device(Inode* parent, Device* device, const char* name, size_t namelen) {
    if(parent->kind != INODE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* inode=device_new(device, name, namelen);
    if(!inode) return -NOT_ENOUGH_MEM;
    intptr_t e;
    if((e=tmpfs_put(parent->priv, inode)) < 0) {
        tmpfs_inode_destroy(inode);
        return e;
    }
    return 0;
}
