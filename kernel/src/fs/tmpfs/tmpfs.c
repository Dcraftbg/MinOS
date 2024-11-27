// TODO: Use Cache for the TmpfsInode instead of TmpfsNodeBlock
#include "tmpfs.h"
#include "../../memory.h"
#include "../../string.h"
#include "../../kernel.h"
#define TMPFS_DIR_NODES 16
// ((PAGE_SIZE-sizeof(void*))/sizeof(Inode))
#define TMPFS_MALLOC(size) kernel_malloc(size)
#define TMPFS_DEALLOC(ptr, size) kernel_dealloc(ptr, size)

struct TmpfsFileBlock;
struct TmpfsNodeBlock;
typedef struct {
    size_t size;
    struct TmpfsFileBlock* first;
} TmpfsFile;

typedef struct {
    size_t size;
    struct TmpfsNodeBlock* first;
    struct TmpfsNodeBlock* last;
} TmpfsDir;

typedef struct {
    Device* device;
} TmpfsDevice;
typedef struct {
    inodekind_t kind;
    char name[MAX_INODE_NAME];
    union {
        TmpfsDir dir;
        TmpfsFile file;
        TmpfsDevice device;
    } data;
} TmpfsInode;

typedef struct TmpfsNodeBlock {
    TmpfsInode inodes[TMPFS_DIR_NODES];
    struct TmpfsNodeBlock* next;
    struct TmpfsNodeBlock* previous;
} TmpfsNodeBlock;

typedef struct TmpfsFileBlock {
    struct TmpfsFileBlock* next;
    char data[PAGE_SIZE-sizeof(struct TmpfsFileBlock*)];
} TmpfsFileBlock;

static TmpfsInode* new_tmpfs_inode() {
    TmpfsInode* buf = (TmpfsInode*)TMPFS_MALLOC(sizeof(*buf));
    if(buf) memset(buf, 0, sizeof(*buf));
    return buf;
}
static TmpfsNodeBlock* new_tmpfs_node_block() {
    TmpfsNodeBlock* buf = (TmpfsNodeBlock*)TMPFS_MALLOC(sizeof(*buf));
    if(buf) memset(buf, 0, sizeof(*buf));
    return buf;
}

static TmpfsFileBlock* new_tmpfs_file_block() {
    TmpfsFileBlock* buf = (TmpfsFileBlock*)TMPFS_MALLOC(sizeof(*buf));
    if(buf) memset(buf, 0, sizeof(*buf));
    return buf;
}
static TmpfsInode* dir_create_inode(TmpfsDir* dir) {
   if(dir->last) {
        if(dir->size % TMPFS_DIR_NODES == 0) {
             assert(!dir->last->next);
             dir->last->next = new_tmpfs_node_block();
             if(!dir->last->next) {
                return NULL;
             }
             dir->last->next->previous = dir->last;
             TmpfsInode* result = &dir->last->next->inodes[0];
             dir->size++;
             return result;
        }
        TmpfsInode* result = &dir->last->inodes[dir->size%TMPFS_DIR_NODES];
        dir->size++;
        return result;
   }
   dir->last = new_tmpfs_node_block();
   dir->first = dir->last;
   if(!dir->last) {
        return NULL;
   }
   TmpfsInode* result = &dir->last->inodes[0];
   dir->size++;
   return result;
}
#if 0
static void dir_remove_inode(TmpfsDir* dir, TmpfsInode* inode) {
    (void)inode;
    if(dir->size % TMPFS_DIR_NODES == 0) {
        void* ptr = dir->last;
        dir->last = dir->last->previous;
        delete_tmpfs_node_block(ptr);
        if(dir->last) dir->last->next = NULL;
    }
    dir->size--;
}
#endif
static FsOps tmpfs_fsops = {0};
static FsOps tmpfs_device_fsops = {0};
static InodeOps tmpfs_inodeops = {0};
intptr_t init_tmpfs(Superblock* sb);
intptr_t deinit_tmpfs(Superblock* sb);
FsDriver tmpfs_driver = {
    &tmpfs_fsops,
    &tmpfs_inodeops,
    init_tmpfs,
    deinit_tmpfs,
};
static void inode_constr(Inode* inode) {
    inode->ops = &tmpfs_inodeops;
}
static void direntry_constr(VfsDirEntry* entry, TmpfsInode* private, Superblock* superblock) {
    vfsdirentry_constr(entry, superblock, (&tmpfs_fsops), private->kind, ((inodeid_t)private), private);
}
// Special function for creating a device
intptr_t tmpfs_register_device(VfsDir* dir, Device* device, VfsDirEntry* result) {
    if(!dir || !dir->private || !device || !result) return -INVALID_PARAM;
    TmpfsInode* inode = dir_create_inode(dir->private);
    if(!inode) return -NOT_ENOUGH_MEM;
    inode->kind = INODE_DEVICE;
    inode->data.device.device = device;
    direntry_constr(result, inode, dir->inode->superblock);
    return 0;
}

intptr_t tmpfs_get_inode(Superblock* superblock, inodeid_t id, Inode** result) {
    if(!result) return -INVALID_PARAM;
    Inode* inode = (*result = iget(vfs_new_inode()));
    TmpfsInode* privInode = (TmpfsInode*)id;
    if(privInode->kind == INODE_DEVICE) {
        inode->superblock = superblock;
        inode->inodeid = id;
        if(privInode->data.device.device->init_inode) return privInode->data.device.device->init_inode(privInode->data.device.device, inode);
        return -BAD_DEVICE;
    }
    inode_constr(inode);
    inode->superblock = superblock;
    inode->private = privInode;
    inode->kind = privInode->kind;
    inode->ops = &tmpfs_inodeops;
    inode->inodeid = id;
    return 0;
}

// Inode Ops
intptr_t tmpfs_open(Inode* this, VfsFile* result, fmode_t mode) {
    if(!this || !this->private) return -BAD_INODE;
    if(this->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    TmpfsInode* tmpfs_inode = (TmpfsInode*)this->private;
    assert(tmpfs_inode->kind == INODE_FILE);
    vfsfile_constr(result, this, &tmpfs_fsops, 0, &((TmpfsInode*)this->private)->data.file, mode);
    return 0;
}
intptr_t tmpfs_diropen(Inode* this, VfsDir* result, fmode_t mode) {
    (void)mode;
    if(!this || !this->private) return -BAD_INODE;
    if(this->kind != INODE_DIR) return -IS_NOT_DIRECTORY;
    TmpfsInode* tmpfs_inode = (TmpfsInode*)this->private;
    assert(tmpfs_inode->kind == INODE_DIR);
    result->ops = &tmpfs_fsops;
    
    result->private = &tmpfs_inode->data.dir;
    result->inode = this;
    return 0;
}
intptr_t tmpfs_rename(VfsDirEntry* this, const char* name, size_t namelen) {
    if(!this || !this->private) return -BAD_INODE;
    if(!name) return -INVALID_PARAM;
    if(namelen >= MAX_INODE_NAME) return -LIMITS;
    TmpfsInode* inode = (TmpfsInode*)this->private;
    memcpy(inode->name, name, namelen);
    inode->name[namelen] = '\0'; // Null terminator
    return 0;
}

intptr_t tmpfs_identify(VfsDirEntry* this, char* namebuf, size_t namecap) {
    if(!this || !this->private) return -BAD_INODE;
    if(!namebuf) return -INVALID_PARAM;
    TmpfsInode* inode = (TmpfsInode*)this->private;
    size_t namelen = strlen(inode->name);
    if(namecap < namelen) return -LIMITS;
    memcpy(namebuf, inode->name, namelen);
    namebuf[namelen] = '\0'; // Null terminator
    return namelen;
}
intptr_t tmpfs_stat(Inode* this, Stats* stats) {
    if(!this || !this->private) return -BAD_INODE;
    TmpfsInode* inode = (TmpfsInode*)this->private;
    switch(inode->kind) {
    case INODE_DIR: {
        memset(stats, 0, sizeof(*stats));
        // Explicit declarations
        stats->lba = PAGE_SHIFT;
        stats->size = (inode->data.dir.size+(TMPFS_DIR_NODES-1))/TMPFS_DIR_NODES;
        return 0;
    } break;
    case INODE_FILE: {
        memset(stats, 0, sizeof(*stats));
        // Explicit declarations
        stats->lba = 0;
        stats->size = inode->data.file.size;
        return 0;
    } break;
    }
    return -UNSUPPORTED;
}
// Fs Ops
intptr_t tmpfs_create(VfsDir* parent, VfsDirEntry* this) {
    if(!parent || !parent->private) return -BAD_INODE;
    if(!this) return -INVALID_PARAM;
    TmpfsDir* dir = (TmpfsDir*)parent->private;
    TmpfsInode* inode = dir_create_inode(dir);
    if(!inode) return -NOT_ENOUGH_MEM;
    inode->kind = INODE_FILE; // A copy, just in case
    memset(&inode->data.file, 0, sizeof(inode->data.dir));
    direntry_constr(this, inode, parent->inode->superblock);
    return 0;
}
intptr_t tmpfs_mkdir(VfsDir* parent, VfsDirEntry* this) {
    if(!this) return -INVALID_PARAM;
    if(!parent->private) return -BAD_INODE;
    TmpfsDir* dir = (TmpfsDir*)parent->private;
    TmpfsInode* inode = dir_create_inode(dir);
    if(!inode) return -NOT_ENOUGH_MEM;
    memset(&inode->data.dir, 0, sizeof(inode->data.dir));
    inode->kind = INODE_DIR; // A copy, just in case
    direntry_constr(this, inode, parent->inode->superblock);
    return 0;
}
intptr_t tmpfs_get_dir_entries(VfsDir* dir, VfsDirEntry* entries, size_t count, off_t off) {
    if(!dir || !dir->private || !entries) return -INVALID_PARAM;
    TmpfsDir* tmpfs_dir = dir->private;
    if(off >= tmpfs_dir->size) return 0;
    size_t i = 0;
    size_t left = tmpfs_dir->size-off;
    TmpfsNodeBlock* block = tmpfs_dir->first;
    while(off >= TMPFS_DIR_NODES && block) {
        block = block->next;
        off -= TMPFS_DIR_NODES;
        left -= TMPFS_DIR_NODES;
    }
    if(!block) return -INVALID_OFFSET;
    if(count > left) count = left;
    while(i < count) {
        direntry_constr(&entries[i], &block->inodes[off], dir->inode->superblock);
        off++;
        i++;
        if(off == TMPFS_DIR_NODES) {
            block = block->next;
            if(!block) return i;
            off = 0;
        }
    }
    return i;
}
intptr_t tmpfs_read(VfsFile* vfsfile, void* buf, size_t size, off_t offset) {
    if(!vfsfile || !vfsfile->private) return -BAD_INODE;
    if(!buf) return -INVALID_PARAM;
    TmpfsFile* file = (TmpfsFile*)vfsfile->private;
    if(offset >= file->size || size == 0) return 0; // We couldn't read anything
    size_t left = file->size-offset;
    size = size < left ? size : left; // size = min(size,left);
    TmpfsFileBlock* block = file->first;
    while(offset >= sizeof(block->data) && block) {
        offset-=sizeof(block->data);
        block = block->next;
    }
    if(offset >= sizeof(block->data)) return -FILE_CORRUPTION;

    size_t ressize = 0; // Count bytes read, in case our block list ends prematurely
    while(block && size) {
        // min(size, sizeof(block->data));
        size_t read_bytes = size < (sizeof(block->data)-offset) ? size : (sizeof(block->data)-offset);
        memcpy(buf, block->data+offset, read_bytes);
        size-=read_bytes;
        buf = (uint8_t*)buf+read_bytes;
        ressize+=read_bytes;
        block = block->next;
        offset = 0;
    }
    return ressize;
}

intptr_t tmpfs_seek(VfsFile* file, off_t offset, seekfrom_t from) {
    if(!file || !file->private) return -INVALID_PARAM;
    TmpfsFile* tmpfs_file = file->private;
    switch(from) {
        case SEEK_START:
            if(offset < 0 || offset > tmpfs_file->size) return -INVALID_OFFSET; 
            file->cursor = offset;
            return file->cursor;
        case SEEK_CURSOR:
            if(offset < 0 && (-offset) > file->cursor) return -INVALID_OFFSET;
            file->cursor += offset;
            return file->cursor;
        case SEEK_EOF:
            if(offset > 0) return -INVALID_OFFSET;
            if(offset < 0 && (-offset) > tmpfs_file->size) return -INVALID_OFFSET;
            file->cursor = tmpfs_file->size-((size_t)(-offset));
            return file->cursor;
        default:
            return -INVALID_PARAM;
    }
}
intptr_t tmpfs_write(VfsFile* vfsfile, const void* buf, size_t size, off_t offset) {
    if(!vfsfile || !vfsfile->private) return -BAD_INODE;
    if(!buf) return -INVALID_PARAM;
    TmpfsFile* file = (TmpfsFile*)vfsfile->private;
    if(offset > file->size || size==0) {
        return 0; // Can't write anything if we're out of bounds 
    }
    if(!file->first) {
        file->first = new_tmpfs_file_block(); // Create first block
    }
    off_t orgoffset = offset;
    size_t total = 0;
    TmpfsFileBlock* block = file->first; 
    TmpfsFileBlock* previous = block; 
    while(offset >= sizeof(block->data) && block) {
        offset-=sizeof(block->data);
        previous = block;
        block = block->next;
    }
    if(offset >= sizeof(block->data)) return -FILE_CORRUPTION;
    if(size) {
        size_t write_bytes = size < (sizeof(block->data)-offset) ? size : (sizeof(block->data)-offset);
        if(!block) {
            block = new_tmpfs_file_block();
            if(!block) goto END;
            if(previous) previous->next = block;
        }
        memcpy(block->data+offset, buf, write_bytes);
        buf = (uint8_t*)buf+write_bytes;
        size -= write_bytes;
        total += write_bytes;
        previous = block;
        block = block->next;
    }
    while(size) {
        size_t write_bytes = size < sizeof(block->data) ? size : sizeof(block->data);
        if(!block) {
            previous->next = new_tmpfs_file_block();
            if(!previous->next) goto END;
            block = previous->next;
        }
        memcpy(block->data, buf, write_bytes);
        buf = (uint8_t*)buf+write_bytes;
        size  -= write_bytes;
        total += write_bytes;
        previous = block;
        block = block->next;
    }
END:
    if(orgoffset+total > file->size) {
        file->size = orgoffset+total;
    }
    return total;
}

void tmpfs_close(VfsFile* file) {
    file->private = NULL;
    file->cursor = 0;
}
void tmpfs_dirclose(VfsDir* dir) {
    dir->private = NULL;
}
// Initialization
intptr_t init_tmpfs(Superblock* sb) {
    memset(&tmpfs_inodeops, 0, sizeof(tmpfs_inodeops));
    memset(&tmpfs_fsops, 0, sizeof(tmpfs_fsops));
    memset(&tmpfs_device_fsops, 0, sizeof(tmpfs_device_fsops));

    tmpfs_inodeops.open = tmpfs_open;
    tmpfs_inodeops.diropen = tmpfs_diropen;
    tmpfs_inodeops.stat = tmpfs_stat;

    tmpfs_fsops.create = tmpfs_create;
    tmpfs_fsops.mkdir = tmpfs_mkdir;
    tmpfs_fsops.get_dir_entries = tmpfs_get_dir_entries;

    tmpfs_fsops.identify = tmpfs_identify;
    tmpfs_fsops.rename = tmpfs_rename;

    tmpfs_fsops.read = tmpfs_read;
    tmpfs_fsops.seek = tmpfs_seek;
    tmpfs_fsops.write = tmpfs_write;

    tmpfs_fsops.close = tmpfs_close;
    tmpfs_fsops.dirclose = tmpfs_dirclose;

    sb->get_inode = tmpfs_get_inode;
    TmpfsInode* root_inode = new_tmpfs_inode();
    if(!root_inode) return -NOT_ENOUGH_MEM;
    root_inode->kind = INODE_DIR;
    memset(&root_inode->data.dir, 0, sizeof(root_inode->data.dir));
    sb->root = (inodeid_t)root_inode;
    return 0;
}
intptr_t deinit_tmpfs(Superblock* sb) {
    (void)sb;
    return 0;
}
