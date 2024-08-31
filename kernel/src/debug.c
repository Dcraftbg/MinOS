#include "debug.h"
#include "fs/tmpfs/tmpfs.h"
#include "kernel.h"
void dump_memmap(Memmap* map) {
    printf("Memory map %p; pages: %zu; available: %zu;\n",map->addr,map->page_count,map->page_available);
    for(size_t i = 0; i < (map->page_count+7)/8; ++i) {
        printf("%02X",map->addr[i]);
        if((i+1) % 8 == 0) printf(";");
        if((i+1) % 32 == 0) printf("\n");
    }
}
// @DEBUG
void log_slab(void* p) {
    Slab* slab = p;
    printf("  Cache: %p\n"       ,slab->cache);
    printf("  memory: %p\n"      ,slab->memory);
    printf("  Objects:\n");
    for(size_t i = 0; i < slab->free; i++) {
        size_t obj_index = slab_bufctl(slab)[i];
        void* obj = slab->memory + obj_index*slab->cache->objsize;
        // NOTE: We expect its an inode
        Inode* inode = obj;
        printf("  -  %p\n",inode);
        printf("     Shared: %zu\n",inode->shared);
        printf("     Kind: %s\n",inode->kind == INODE_FILE ? "file" : "dir");
        printf("     Private: %p\n",inode->private);
        tmpfs_log(inode, 7);
    }
}
// @DEBUG
void log_list(struct list* list, void (*log_obj)(void* obj)) {
    struct list* first = list;
    list = list->next; // Always skip the first one in this case, since cache.full/partial/free is not a valid Slab
    while(first != list) {
        printf("- %p:\n",list);
        if(log_obj) log_obj(list);
        list = list->next;
    }
}
// @DEBUG
void log_cache(Cache* cache) {
    printf("Cache:\n");
    printf("Objects: %zu/%zu\n",cache->inuse,cache->totalobjs);
    printf("Object Size: %zu\n",cache->objsize);
    printf("Object Per Slab: %zu\n",cache->objs_per_slab);
    printf("Full:\n");
    log_list(&cache->full, log_slab);
    printf("Partial:\n");
    log_list(&cache->partial, log_slab);
    printf("Free:\n");
    log_list(&cache->free, log_slab);
}




void cat(const char* path) {
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


void hexdump_mem(uint8_t* buf, size_t size) {
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
}

void hexdump(const char* path) {
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
    hexdump_mem(buf, size);
    kernel_dealloc(buf,size);
    vfs_close(&file);
}


static const char* inode_kind_map[] = {
    "DIR",
    "FILE",
    "DEVICE",
};
static_assert(ARRAY_LEN(inode_kind_map) == INODE_COUNT, "Update inode_kind_map");

void ls(const char* path) {
    printf("%s:\n",path);
    VfsDir dir = {0};
    VfsDirIter iter = {0};
    VfsDirEntry entry = {0};
    intptr_t e = 0;
    char namebuf[MAX_INODE_NAME];
    if ((e=vfs_diropen(path, &dir, MODE_READ)) < 0) {
        printf("ERROR: ls: Could not open directory: %s\n", status_str(e));
        return;
    }
    if ((e=vfs_diriter_open(&dir, &iter)) < 0) {
        printf("ERROR: ls: Could not open iter: %s\n", status_str(e));
        vfs_dirclose(&dir);
        return;
    }

    VfsStats stats = {0};
    while((e = vfs_diriter_next(&iter, &entry)) == 0) {
        if((e=vfs_identify(&entry, namebuf, sizeof(namebuf))) < 0) {
            printf("ERROR: ls: Could not identify inode: %s\n",status_str(e));
            vfs_diriter_close(&iter);
            vfs_dirclose(&dir);
            return;
        }
        if((e=vfs_stat(&entry, &stats)) < 0) {
            printf("WARN: ls: Could not get stats for %s: %s\n",namebuf,status_str(e));
            continue;
        }
        printf("%6s %15s %zu bytes \n", inode_kind_map[entry.kind], namebuf, stats.size * (1<<stats.lba));
    }
    if(e != -NOT_FOUND) {
        printf("ERROR: ls: Failed to iterate: %ld\n",e);
        vfs_diriter_close(&iter);
        vfs_dirclose(&dir);
        return;
    }
    vfs_diriter_close(&iter);
    vfs_dirclose(&dir);
}


void dump_inodes(Superblock* superblock) {
    // debug_assert(file->inode);
    InodeMap* map = &superblock->inodemap;
    printf("Inode Dump:\n");
    printf("Amount: %zu\n",map->len);
    for(size_t i = 0; i < map->buckets.len; ++i) {
        Pair_InodeMap* pair = map->buckets.items[i].first;
        while(pair) {
            printf("%zu:\n", pair->key);
            Inode* inode = pair->value;
            printf(" inodeid = %zu\n",inode->inodeid);
            printf(" shared = %zu\n",inode->shared);
            printf(" mode = %d\n",inode->mode);
            pair = pair->next;
        }
    }
}


void dump_caches() {
    struct list* first = &kernel.cache_list;
    struct list* list = first->next; 
    while(first != list) {
        Cache* cache = (Cache*)list;
        printf("Cache:\n");
        printf("  Name: %s\n",cache->name);
        printf("  Objects: %zu/%zu\n",cache->inuse,cache->totalobjs);
        printf("  Object Size: %zu\n",cache->objsize);
        printf("  Object Per Slab: %zu\n",cache->objs_per_slab);
        printf("  Bytes per slab: %zu\n",PAGE_ALIGN_UP(cache->objs_per_slab * (cache->objsize + sizeof(uint32_t)) + sizeof(Slab)));
        list = list->next;
    }
}

void dump_memregions(struct list* list) {
    printf("Region Dump:\n");
    struct list* first = list;
    list  = list->next;
    while(first != list) {
        MemoryList* memlist = (MemoryList*)list;
        MemoryRegion* region = memlist->region;
        char rflags[2] = {0};
        char pflags[10] = {0};
        page_flags_serialise(region->pageflags, pflags, sizeof(pflags)-1);
        const char* strp_type = page_type_str(region->pageflags);
        rflags[0] = region->flags & MEMREG_WRITE ? 'w' : '-';
        printf("Region:\n");
        printf("   rflags = %s\n",rflags);
        printf("   pflags = %s (%s)\n",pflags, strp_type);
        printf("   address = %p\n",(void*)region->address);
        printf("   pages = %zu\n",region->pages);
        printf("   shared = %zu\n",region->shared);
        list = list->next;
    }
}
