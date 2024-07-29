#include "mmap.h"
#include "slab.h"
#include "list.h"
#include "vfs.h"
#include "string.h"
#include "ctype.h"

void dump_memmap(Memmap* map);
void log_slab(void* p);
void log_list(struct list* list, void (*log_obj)(void* obj));
void log_cache(Cache* cache);

static intptr_t read_exact(VfsFile* file, void* bytes, size_t amount) {
    while(amount) {
        size_t rb = vfs_read(file, bytes, amount);
        if(rb < 0) return rb;
        if(rb == 0) return -PREMATURE_EOF;
        amount-=rb;
        bytes+=rb;
    }
    return 0;
}
static void cat(const char* path) {
    VfsFile file = {0};
    intptr_t e = 0;
    if((e=vfs_open(path, &file, MODE_READ)) < 0) {
        printf("ERROR: cat: Failed to open %s : %ld\n",path,e);
        return;
    }
    if((e=vfs_seek(&file, 0, SEEK_END)) < 0) {
        printf("ERROR: cat: Could not seek to file end : %ld\n",e);
        vfs_close(&file);
        return;
    }
    size_t size = e;
    if((e=vfs_seek(&file, 0, SEEK_START) < 0)) {
        printf("ERROR: cat: Could not seek to file start : %ld\n",e);
        vfs_close(&file);
        return;
    }
    char* buf = (char*)kernel_malloc(size+1);
    if((e=read_exact(&file, buf, size)) < 0) {
        printf("ERROR: cat: Could not read file contents : %ld\n",e);
        vfs_close(&file);
        return;
    }
    buf[size] = '\0';
    printf("%s\n",buf);
    kernel_dealloc(buf,size+1);
    vfs_close(&file);
}
static void hexdump(const char* path) {
    VfsFile file = {0};
    intptr_t e = 0;
    if((e=vfs_open(path, &file, MODE_READ)) < 0) {
        printf("ERROR: hexdump: Failed to open %s : %ld\n",path,e);
        return;
    }
    if((e=vfs_seek(&file, 0, SEEK_END)) < 0) {
        printf("ERROR: hexdump: Could not seek to file end : %ld\n",e);
        vfs_close(&file);
        return;
    }
    size_t size = e;
    if((e=vfs_seek(&file, 0, SEEK_START) < 0)) {
        printf("ERROR: hexdump: Could not seek to file start : %ld\n",e);
        vfs_close(&file);
        return;
    }
    uint8_t* buf = (uint8_t*)kernel_malloc(size);
    if((e=read_exact(&file, buf, size)) < 0) {
        printf("ERROR: hexdump: Could not read file contents : %ld\n",e);
        vfs_close(&file);
        return;
    }
    uint8_t* prev = NULL;
    bool rep = false;
    for(size_t i = 0; i < size/16; ++i) {
        uint8_t* current = &buf[i*16];
        if(prev == NULL || memcmp(current, prev, 16) != 0) {
           printf("%08X:  ",(uint32_t)i*16);
           for(size_t x = 0; x < 16; ++x) {
              if(x != 0) {
                if(x % 8 == 0) {
                   printf("  ");
                } else {
                   printf(" ");
                }
              }
              uint8_t byte = current[x];
              printf("%02X",byte);
           }
           printf("  |");
           for(size_t x = 0; x < 16; ++x) {
              uint8_t byte = current[x];
              char c = isprint(byte) ? byte : '.';
              printf("%c",c);
           }
           printf("|");
           printf("\n");
           rep = false;
        } else if (!rep) {
           printf("*\n");
           rep = true;
        }
        prev = current;
    }
    if(size%16) {
       size_t i = size-(size%16);
       uint8_t* current = &buf[i];

       printf("%08X:  ",(uint32_t)i);
       size_t left = size-i;
       for(size_t x = 0; x < left; ++x) {
          if(x != 0) {
            if(x % 8 == 0) {
               printf("  ");
            } else {
               printf(" ");
            }
          }
          uint8_t byte = current[x];
          printf("%02X",byte);
       }
       printf("  |");
       for(size_t x = 0; x < left; ++x) {
          uint8_t byte = current[x];
          char c = isprint(byte) ? byte : '.';
          printf("%c",c);
       }
       printf("|");
       printf("\n");
    }
    kernel_dealloc(buf,size);
    vfs_close(&file);
}
static const char* inode_kind_map[] = {
    "DIR",
    "FILE",
    "DEVICE",
};
static_assert(ARRAY_LEN(inode_kind_map) == INODE_COUNT, "Update inode_kind_map");
static void ls(const char* path) {
    printf("%s:\n",path);
    VfsDir dir = {0};
    VfsDirIter iter = {0};
    Inode* item = iget(vfs_new_inode());
    intptr_t e = 0;
    char namebuf[MAX_INODE_NAME];
    if ((e=vfs_diropen(path, &dir)) < 0) {
        printf("ERROR: ls: Could not open directory: %ld\n", e);
        idrop(item);
        return;
    }
    if ((e=vfs_diriter_open(&dir, &iter)) < 0) {
        printf("ERROR: ls: Could not open iter: %ld\n",e);
        vfs_dirclose(&dir);
        idrop(item);
        return;
    }

    while((e = vfs_diriter_next(&iter, item)) == 0) {
        if((e=vfs_identify(item, namebuf, sizeof(namebuf))) < 0) {
            printf("ERROR: ls: Could not identify inode: %ld\n",e);
            vfs_diriter_close(&iter);
            vfs_dirclose(&dir);
            idrop(item);
            return;
        }
        printf("%6s %15s %zu bytes \n", inode_kind_map[item->kind], namebuf, item->size * (1<<item->lba));
    }
    if(e != -NOT_FOUND) {
        printf("ERROR: ls: Failed to iterate: %ld\n",e);
        vfs_diriter_close(&iter);
        vfs_dirclose(&dir);
        idrop(item);
        return;
    }
    vfs_diriter_close(&iter);
    vfs_dirclose(&dir);
    idrop(item);
}
static intptr_t write_exact(VfsFile* file, const void* bytes, size_t amount) {
    while(amount) {
        size_t wb = vfs_write(file, bytes, amount);
        if(wb < 0) return wb;
        amount-=wb;
        bytes+=wb;
    }
    return 0;
}
