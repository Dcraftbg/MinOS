#pragma once
#include "mem/bitmap.h"
#include "mem/slab.h"
#include <collections/list.h>
#include "vfs.h"
#include "string.h"
#include "ctype.h"
#include "mem/memregion.h"
#include "fileutils.h"

void dump_bitmap(Bitmap* map);
void log_slab(void* p);
void log_list(struct list* list, void (*log_obj)(void* obj));
void log_cache(Cache* cache);

void cat(const char* path);
void hexdump_mem(uint8_t* buf, size_t size);
void hexdump(const char* path);
void ls(const char* path);

void dump_memregions(struct list* list);
void dump_caches();
void dump_inodes(Superblock* superblock);

void dump_pml4_perms(page_t pml4, pageflags_t flags_ignore);
void dump_pml4_diff (page_t s1_pml4, page_t s2_pml4, pageflags_t flags_ignore);
