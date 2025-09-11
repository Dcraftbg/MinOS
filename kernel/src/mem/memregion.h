#pragma once
#include <list_head.h>
#include "utils.h"
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include "page.h"
typedef struct {
    pageflags_t pageflags;
    uintptr_t address;
    size_t pages;
    atomic_size_t shared;
} MemoryRegion;
typedef struct MemoryList {
    struct list_head list;
    MemoryRegion* region;
} MemoryList;
void init_memregion(); 
MemoryRegion* memregion_new(pageflags_t pageflags, uintptr_t address, size_t pages);
MemoryRegion* memregion_clone(MemoryRegion* region, page_t src, page_t dst);
MemoryList* memlist_new(MemoryRegion* region); 
void memregion_drop(MemoryRegion* region, page_t pml4);
void memlist_dealloc(MemoryList* list, page_t pml4); 
void memlist_add(struct list_head *list, MemoryList *new);
MemoryList* memlist_find_available(struct list_head *list, MemoryRegion* result, void* post_addr, size_t minsize_pages, size_t maxsize_pages);
MemoryList* memlist_find(struct list_head *list, void* address);

