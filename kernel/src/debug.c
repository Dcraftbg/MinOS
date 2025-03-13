#include "debug.h"
#include "fs/tmpfs/tmpfs.h"
#include "kernel.h"
#include "log.h"

void dump_bitmap(Bitmap* map) {
    printf("Memory map %p; pages: %zu; available: %zu;\n",map->addr,map->page_count,map->page_available);
    for(size_t i = 0; i < (map->page_count+7)/8; ++i) {
        printf("%02X",map->addr[i]);
        if((i+1) % 8 == 0) printf(";");
        if((i+1) % 32 == 0) printf("\n");
    }
}
void cat(const char* path) {
    Inode* file;
    intptr_t e = 0;
    if((e=vfs_find_abs(path, &file)) < 0) {
        kwarn("cat: Failed to open %s : %s", path, status_str(e));
        return;
    }
    if((e=inode_size(file)) < 0) {
        kwarn("cat: Could not seek to file end : %s", status_str(e));
        idrop(file);
        return;
    }
    size_t size = e;
    char* buf = (char*)kernel_malloc(size+1);
    if((e=read_exact(file, buf, size, 0)) < 0) {
        kwarn("cat: Could not read file contents : %s", status_str(e));
        idrop(file);
        return;
    }
    buf[size] = '\0';
    kinfo("%s",buf);
    kernel_dealloc(buf,size+1);
    idrop(file);
}


void hexdump_mem(uint8_t* buf, size_t size) {
    uint8_t* prev = NULL;
    bool rep = false;
    for(size_t i = 0; i < size/16; ++i) {
        uint8_t* current = &buf[i*16];
        if(prev == NULL || memcmp(current, prev, 16) != 0) {
           printf("%08x  ",(uint32_t)i*16);
           for(size_t x = 0; x < 16; ++x) {
              if(x != 0) {
                if(x % 8 == 0) {
                   printf("  ");
                } else {
                   printf(" ");
                }
              }
              uint8_t byte = current[x];
              printf("%02x",byte);
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

       printf("%08x  ",(uint32_t)i);
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
          printf("%02x",byte);
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
    Inode* file;
    intptr_t e = 0;
    if((e=vfs_find_abs(path, &file)) < 0) {
        kerror("hexdump: Failed to open %s : %ld",path,e);
        return;
    }
    if((e=inode_size(file)) < 0) {
        kerror("hexdump: Could not seek to file end : %ld",e);
        idrop(file);
        return;
    }
    size_t size = e;
    uint8_t* buf = (uint8_t*)kernel_malloc(size);
    if((e=read_exact(file, buf, size, 0)) < 0) {
        kerror("hexdump: Could not read file contents : %ld",e);
        idrop(file);
        return;
    }
    hexdump_mem(buf, size);
    kernel_dealloc(buf,size);
    idrop(file);
}


static const char* inode_kind_map[] = {
    "DIR",
    "FILE",
    "DEVICE",
    "SOCKET"
};
static_assert(ARRAY_LEN(inode_kind_map) == INODE_COUNT, "Update inode_kind_map");

// TODO: If you need it, you'll need to rewrite ls in the kernel.
// Its stupid tho I don't use it anymore

void dump_inodes(Superblock* superblock) {
    // debug_assert(file->inode);
    InodeMap* map = &superblock->inodemap;
    kdebug("Inode Dump:");
    kdebug("Amount: %zu",map->len);
    for(size_t i = 0; i < map->buckets.len; ++i) {
        Pair_InodeMap* pair = map->buckets.items[i].first;
        while(pair) {
            kdebug("%zu:", pair->key);
            Inode* inode = pair->value;
            kdebug(" inodeid = %zu",inode->id);
            kdebug(" shared = %zu",inode->shared);
            kdebug(" mode = %d",inode->mode);
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
        printf("  Bytes per slab: %zu\n", (size_t)PAGE_ALIGN_UP(cache->objs_per_slab * (cache->objsize + sizeof(uint32_t)) + sizeof(Slab)));
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

// NOTE: Temporary diff function. Will probably be removed later on
void dump_pml4_diff(page_t s1_pml4, page_t s2_pml4, pageflags_t flags_ignore) {
    size_t pml4=0, pml3=0, pml2=0, pml1=0;
#define dump_pml4_diff_if(a,b,at) \
    if(a[at] == 0 && b[at] == 0) continue;\
    if(a[at] == 0) {\
        printf("<Hole> in s1 (" #a "): %zu : %zu : %zu : %zu\n", pml4, pml3, pml2, pml1);\
        continue;\
    } else if (b[at] == 0) {\
        printf("<Hole> in s2 (" #b "): %zu : %zu : %zu : %zu\n", pml4, pml3, pml2, pml1);\
        continue;\
    }
    char s1_pflags[10] = {0};
    char s2_pflags[10] = {0};

    for(pml4 = 0; pml4 < KERNEL_PAGE_ENTRIES; ++pml4) {
         dump_pml4_diff_if(s1_pml4, s2_pml4, pml4);

         page_t s1_pml3 = (page_t)PAGE_ALIGN_DOWN(s1_pml4[pml4] | KERNEL_MEMORY_MASK);
         page_t s2_pml3 = (page_t)PAGE_ALIGN_DOWN(s2_pml4[pml4] | KERNEL_MEMORY_MASK);

         for(pml3 = 0; pml3 < KERNEL_PAGE_ENTRIES; ++pml3) {
              dump_pml4_diff_if(s1_pml3, s2_pml3, pml3);

              page_t s1_pml2 = (page_t)PAGE_ALIGN_DOWN(s1_pml3[pml3] | KERNEL_MEMORY_MASK);
              page_t s2_pml2 = (page_t)PAGE_ALIGN_DOWN(s2_pml3[pml3] | KERNEL_MEMORY_MASK);

              for(pml2 = 0; pml2 < KERNEL_PAGE_ENTRIES; ++pml2) {
                   dump_pml4_diff_if(s1_pml2, s2_pml2, pml2);

                   page_t s1_pml1 = (page_t)PAGE_ALIGN_DOWN(s1_pml2[pml2] | KERNEL_MEMORY_MASK);
                   page_t s2_pml1 = (page_t)PAGE_ALIGN_DOWN(s2_pml2[pml2] | KERNEL_MEMORY_MASK);

                   for(pml1 = 0; pml1 < KERNEL_PAGE_ENTRIES; ++pml1) {
                        dump_pml4_diff_if(s1_pml1, s2_pml1, pml1);

                        uint64_t s1_entry = s1_pml1[pml1];
                        uint64_t s2_entry = s2_pml1[pml2];

                        pageflags_t s1_flags = (s1_entry & (0b111111111111 | KERNEL_PFLAG_EXEC_DISABLE)) & (~(flags_ignore | BIT(7)));
                        pageflags_t s2_flags = (s2_entry & (0b111111111111 | KERNEL_PFLAG_EXEC_DISABLE)) & (~(flags_ignore | BIT(7)));

                        uintptr_t s1_phys = PAGE_ALIGN_DOWN(s1_entry) & (~(KERNEL_PFLAG_EXEC_DISABLE));
                        uintptr_t s2_phys = PAGE_ALIGN_DOWN(s2_entry) & (~(KERNEL_PFLAG_EXEC_DISABLE));

                        if(s1_flags != s2_flags) {
                            page_flags_serialise(s1_flags, s1_pflags, sizeof(s1_pflags)-1);
                            page_flags_serialise(s2_flags, s2_pflags, sizeof(s2_pflags)-1);
                            printf("<Flags> in s1 %s in s2 %s: %zu : %zu : %zu : %zu\n", s1_pflags, s2_pflags, pml4, pml3, pml2, pml1);
                            continue;
                        }
                        void* s1_virt = (void*)(s1_phys | KERNEL_MEMORY_MASK);
                        void* s2_virt = (void*)(s2_phys | KERNEL_MEMORY_MASK);
                        if(s1_virt == s2_virt) continue;
                        if(memcmp(s1_virt, s2_virt, PAGE_SIZE) != 0) {
                            printf("<Data> in s1 in s2: %zu : %zu : %zu : %zu\n", pml4, pml3, pml2, pml1);
                            continue;
                        }
                   }
                   pml1 = 0;
              }
              pml2 = 0;
         }
         pml3 = 0;
    }
}

// NOTE: Temporary perms function. Will probably be removed later on
void dump_pml4_perms(page_t pml4_addr, pageflags_t flags_ignore) {
    printf("dump_pml4_perms(%p)\n",pml4_addr);
    uintptr_t since=0;
    uintptr_t at=0;
    pageflags_t cflags=0;
    pageflags_t flags=0;
    char perm_buf[10];

    uintptr_t pml4=0, pml3=0, pml2=0, pml1=0;
#define dump_pml4_perms_empty() \
    do {\
        at = (pml4 << (12+27)) | (pml3 << (12+18)) | (pml2 << (12+9)) | (pml1<<12);\
        if(cflags) {\
            page_flags_serialise(cflags, perm_buf, sizeof(perm_buf));\
            printf("  %p -> %p %s (%s)\n", (void*)since, (void*)at, perm_buf, page_type_str(cflags));\
        }\
        since = at;\
        cflags = flags = 0;\
    } while(0)
    for(pml4 = 0; pml4 < KERNEL_PAGE_ENTRIES; ++pml4) {
         if(pml4_addr[pml4] == 0) {
             dump_pml4_perms_empty();
             continue;
         }
         page_t pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4] | KERNEL_MEMORY_MASK);

         for(pml3 = 0; pml3 < KERNEL_PAGE_ENTRIES; ++pml3) {
              if(pml3_addr[pml3] == 0) {
                  dump_pml4_perms_empty();
                  continue;
              }
              page_t pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3] | KERNEL_MEMORY_MASK);

              for(pml2 = 0; pml2 < KERNEL_PAGE_ENTRIES; ++pml2) {
                   if(pml2_addr[pml2] == 0) {
                       dump_pml4_perms_empty();
                       continue;
                   }
                   page_t pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2] | KERNEL_MEMORY_MASK);

                   for(pml1 = 0; pml1 < KERNEL_PAGE_ENTRIES; ++pml1) {
                        if(pml1_addr[pml1] == 0) {
                            dump_pml4_perms_empty();
                            continue;
                        }
                        flags = (pml1_addr[pml1] & (0b111111111111 | KERNEL_PFLAG_EXEC_DISABLE)) & (~(flags_ignore | BIT(7)));
                        at = (pml4 << (12+27)) | (pml3 << (12+18)) | (pml2 << (12+9)) | (pml1<<12);
                        if(cflags != flags) {
                            if(cflags) {
                                page_flags_serialise(cflags, perm_buf, sizeof(perm_buf));
                                printf("  %p -> %p %s (%s)\n", (void*)since, (void*)at, perm_buf, page_type_str(cflags));
                            }
                            since = at;
                            cflags = flags;
                        }
                   }
                   pml1 = 0;
              }
              pml2 = 0;
         }
         pml3 = 0;
    }

    if(cflags) {
        page_flags_serialise(cflags, perm_buf, sizeof(perm_buf));
        printf("  %p -> %p %s (%s)\n", (void*)since, (void*)at, perm_buf, page_type_str(cflags));
    }
}
