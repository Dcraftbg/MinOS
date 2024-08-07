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

typedef struct {
    size_t at;
    size_t left;
    struct TmpfsNodeBlock* block;
} TmpfsDirIter;
typedef struct TmpfsNodeBlock {
    TmpfsInode inodes[TMPFS_DIR_NODES];
    struct TmpfsNodeBlock* next;
    struct TmpfsNodeBlock* previous;
} TmpfsNodeBlock;
typedef struct TmpfsFileBlock {
    struct TmpfsFileBlock* next;
    char data[PAGE_SIZE-sizeof(struct TmpfsFileBlock*)];
} TmpfsFileBlock;
#if 0
static void delete_tmpfs_node_block(TmpfsNodeBlock* block) {
    TMPFS_DEALLOC(block, sizeof(*block));
}
void tmpfs_dump_file(VfsFile* file) {
    TmpfsFile* f = (TmpfsFile*)file->private;
    TmpfsFileBlock* block = f->first;
    printf("File size: %zu\n",f->size);
    while(block) {
        printf("Block: %p\n",block);
        for(size_t i = 0; i < sizeof(block->data); ++i) {
            printf("%02X",block->data[i]);
        }
        printf("\n");
        block = block->next;
    }
}
#endif
static TmpfsDirIter* new_tmpfs_diriter() {
    TmpfsDirIter* buf = (TmpfsDirIter*)TMPFS_MALLOC(sizeof(*buf));
    if(buf) memset(buf, 0, sizeof(*buf));
    return buf;
}

static void delete_tmpfs_diriter(TmpfsDirIter* buf) {
    TMPFS_DEALLOC(buf, sizeof(*buf));
}
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
intptr_t init_tmpfs();
intptr_t deinit_tmpfs();
FsDriver tmpfs_driver = {
    &tmpfs_fsops,
    &tmpfs_inodeops,
    init_tmpfs,
    deinit_tmpfs,
};
static void inode_constr(Inode* inode) {
    inode->lba = 0;
    inode->size = 0;
    inode->ops = &tmpfs_inodeops;
}
// Special function for creating a device
intptr_t tmpfs_register_device(VfsDir* dir, Device* device, Inode* result) {
    if(!dir || !dir->private || !device || !result) return -INVALID_PARAM;
    TmpfsInode* inode = dir_create_inode(dir->private);
    if(!inode) return -NOT_ENOUGH_MEM;
    inode->kind = INODE_DEVICE;
    inode->data.device.device = device;
    inode_constr(result);
    result->private = inode;
    result->kind = inode->kind;
    result->ops = &tmpfs_inodeops;
    result->lba = 0; // TODO: ask device itself
    result->size = 0;
    return 0;
}
// Inode Ops
intptr_t tmpfs_open(Inode* this, VfsFile* result, fmode_t mode) {
    if(!this || !this->private) return -BAD_INODE;
    switch(this->kind) {
    case INODE_FILE: {
        TmpfsInode* tmpfs_inode = (TmpfsInode*)this->private;
        assert(tmpfs_inode->kind == INODE_FILE);
        result->ops = &tmpfs_fsops;
        result->cursor = 0;
        result->private = &((TmpfsInode*)this->private)->data.file;
    } break;
    case INODE_DEVICE: {
        TmpfsInode* tmpfs_inode = (TmpfsInode*)this->private;
        assert(tmpfs_inode->kind == INODE_DEVICE);

        if(tmpfs_inode->data.device.device && tmpfs_inode->data.device.device->open) {
            result->ops = tmpfs_inode->data.device.device->ops;
            return tmpfs_inode->data.device.device->open(tmpfs_inode->data.device.device, result, mode);
        }
        return -BAD_DEVICE;
    } break;
    default:
        return -INVALID_PARAM;
    }
    // Unreachable
    return 0;
}
intptr_t tmpfs_diropen(Inode* this, VfsDir* result) {
    if(!this || !this->private) return -BAD_INODE;
    if(this->kind != INODE_DIR) return -INVALID_PARAM;
    TmpfsInode* tmpfs_inode = (TmpfsInode*)this->private;
    assert(tmpfs_inode->kind == INODE_DIR);
    result->ops = &tmpfs_fsops;
    result->private = &tmpfs_inode->data.dir;
    return 0;
}
intptr_t tmpfs_rename(Inode* this, const char* name, size_t namelen) {
    if(!this || !this->private) return -BAD_INODE;
    if(!name) return -INVALID_PARAM;
    if(namelen >= MAX_INODE_NAME) return -LIMITS;
    TmpfsInode* inode = (TmpfsInode*)this->private;
    memcpy(inode->name, name, namelen);
    inode->name[namelen] = '\0'; // Null terminator
    return 0;
}

intptr_t tmpfs_identify(Inode* this, char* namebuf, size_t namecap) {
    if(!this || !this->private) return -BAD_INODE;
    if(!namebuf) return -INVALID_PARAM;
    TmpfsInode* inode = (TmpfsInode*)this->private;
    size_t namelen = strlen(inode->name);
    if(namecap < namelen) return -LIMITS;
    memcpy(namebuf, inode->name, namelen);
    namebuf[namelen] = '\0'; // Null terminator
    return 0;
}
// Fs Ops
intptr_t tmpfs_create(VfsDir* parent, Inode* this) {
    if(!parent || !parent->private) return -BAD_INODE;
    if(!this) return -INVALID_PARAM;
    TmpfsDir* dir = (TmpfsDir*)parent->private;
    TmpfsInode* inode = dir_create_inode(dir);
    if(!inode) return -NOT_ENOUGH_MEM;
    inode_constr(this);
    this->kind = INODE_FILE;
    inode->kind = INODE_FILE; // A copy, just in case
    memset(&inode->data.file, 0, sizeof(inode->data.dir));
    this->private = inode;
    return 0;
}
intptr_t tmpfs_mkdir(VfsDir* parent, Inode* this) {
    if(!this) return -INVALID_PARAM;

    this->kind = INODE_DIR;
    inode_constr(this);

    if(!parent) { 
        // Initializing root node
        TmpfsInode* inode = new_tmpfs_inode();
        if(!inode) return -NOT_ENOUGH_MEM;
        inode->kind = INODE_DIR;
        this->private = inode;
        memset(&inode->data.dir, 0, sizeof(inode->data.dir));
        return 0;
    }
    if(!parent->private) return -BAD_INODE;
    TmpfsDir* dir = (TmpfsDir*)parent->private;
    TmpfsInode* inode = dir_create_inode(dir);
    if(!inode) return -NOT_ENOUGH_MEM;
    memset(&inode->data.dir, 0, sizeof(inode->data.dir));
    inode->kind = INODE_DIR; // A copy, just in case
    this->private = inode;
    return 0;
}
intptr_t tmpfs_diriter_open(VfsDir* dir, VfsDirIter* result) {
    if(!dir || !dir->private) return -BAD_INODE;
    TmpfsDir* tmpfsdir = (TmpfsDir*)dir->private;
    TmpfsDirIter* iter = new_tmpfs_diriter();
    if(!iter) return -NOT_ENOUGH_MEM;
    iter->at = 0;
    iter->left = tmpfsdir->size;
    iter->block = tmpfsdir->first;
    result->private = iter;
    result->ops = &tmpfs_fsops;
    return 0;
}
intptr_t tmpfs_diriter_next(VfsDirIter* iter, Inode* result) {
    if(!iter || !iter->private || !result) return -INVALID_PARAM;
    TmpfsDirIter* tmpfs_iter = (TmpfsDirIter*)iter->private;
    if(tmpfs_iter->left == 0) return -NOT_FOUND;
    if(tmpfs_iter->block == NULL) return -BAD_INODE;
    TmpfsInode* node = &tmpfs_iter->block->inodes[tmpfs_iter->at];
    inode_constr(result);
    switch(node->kind) {
    case INODE_FILE:
       result->size = node->data.file.size;
       break;
    }
    result->kind = node->kind;
    result->private = node;
    tmpfs_iter->left--;
    tmpfs_iter->at++;
    if(tmpfs_iter->at == TMPFS_DIR_NODES) {
        tmpfs_iter->at=0;
        tmpfs_iter->block = tmpfs_iter->block->next;
    }
    return 0;
}
intptr_t tmpfs_diriter_close(VfsDirIter* iter) {
    delete_tmpfs_diriter(iter->private);
    iter->private = NULL;
    return 0;
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
        case SEEK_END:
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
#if 0
intptr_t tmpfs_device_open(Inode* this, VfsFile* result) {
    if(!this || !this->private || this->kind != INODE_DEVICE) return -INVALID_PARAM;
    TmpfsInode* inode = this->private;
}
#endif
// Initialization
intptr_t init_tmpfs() {
    memset(&tmpfs_inodeops, 0, sizeof(tmpfs_inodeops));
    memset(&tmpfs_fsops, 0, sizeof(tmpfs_fsops));
    memset(&tmpfs_device_fsops, 0, sizeof(tmpfs_device_fsops));

    tmpfs_inodeops.open = tmpfs_open;
    tmpfs_inodeops.diropen = tmpfs_diropen;
    tmpfs_inodeops.rename = tmpfs_rename;
    tmpfs_inodeops.identify = tmpfs_identify;

    tmpfs_fsops.create = tmpfs_create;
    tmpfs_fsops.mkdir = tmpfs_mkdir;
    tmpfs_fsops.diriter_open = tmpfs_diriter_open;
    tmpfs_fsops.diriter_next = tmpfs_diriter_next;
    tmpfs_fsops.diriter_close = tmpfs_diriter_close;

    tmpfs_fsops.read = tmpfs_read;
    tmpfs_fsops.seek = tmpfs_seek;
    tmpfs_fsops.write = tmpfs_write;

    tmpfs_fsops.close = tmpfs_close;
    tmpfs_fsops.dirclose = tmpfs_dirclose;

#if 0
    tmpfs_device_fsops.open  = tmpfs_device_open ;
    tmfps_device_fsops.read  = tmpfs_device_read ;
    tmfps_device_fsops.write = tmpfs_device_write;
    tmfps_device_fsops.seek  = tmpfs_device_seek ;
    tmpfs_device_fsops.close = tmpfs_device_close;
#endif
    return 0;
}
intptr_t deinit_tmpfs() {
    return 0;
}
#if 1
#define PAD() for(size_t i=0; i < pad; ++i) printf(" ");
void tmpfs_log_file(TmpfsFile* file, size_t pad) {
    PAD() printf("size: %zu\n",file->size);
    PAD() printf("first: %p\n",file->first);
}

void tmpfs_log_dir(TmpfsDir* dir, size_t pad) {
    PAD() printf("size: %zu\n",dir->size);
    PAD() printf("first: %p\n",dir->first);
    PAD() printf("last: %p\n",dir->last);
}

void tmpfs_log(Inode* inode, size_t pad) {
    TmpfsInode* i = (TmpfsInode*)inode->private;
    PAD() printf("Kind: %u\n",i->kind);
    PAD() printf("Name: %s\n",i->name);
    if(i->kind == INODE_FILE) {
        tmpfs_log_file(&i->data.file, pad);
    } else if (i->kind == INODE_DIR) {
        tmpfs_log_dir(&i->data.dir, pad);
    }
}
#endif
