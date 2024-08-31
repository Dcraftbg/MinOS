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
