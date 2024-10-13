#pragma once
#include "list.h"
#include "utils.h"
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include "kernel.h"
enum {
    MEMREG_WRITE=BIT(1),
};
typedef struct {
    uint16_t flags;
    pageflags_t pageflags;
    uintptr_t address;
    size_t pages;
    atomic_size_t shared;
} MemoryRegion;
typedef struct {
    struct list list;
    MemoryRegion* region;
} MemoryList;
void init_memregion(); 
static MemoryRegion* memregion_new(uint16_t flags, pageflags_t pageflags, uintptr_t address, size_t pages) {
    MemoryRegion* region;
    if(!(region=cache_alloc(kernel.memregion_cache))) return NULL;
    region->flags = flags;
    region->pageflags = pageflags;
    region->address = address;
    region->pages = pages;
    region->shared = 1;
    return region;
}
MemoryRegion* memregion_clone(MemoryRegion* region, page_t src, page_t dst);
static void memregion_drop(MemoryRegion* region, page_t pml4) {
    if(region->shared == 1) {
        // if(pml4) page_dealloc(pml4, region->address, region->pages);
        cache_dealloc(kernel.memregion_cache, region);
        return;
    }
    region->shared--;
}
static MemoryList* memlist_new(MemoryRegion* region) {
    if(!region) return NULL;
    MemoryList* list = (MemoryList*)cache_alloc(kernel.memlist_cache);
    if(!list) return NULL;
    list->region = region;
    list_init(&list->list);
    return list;
}
static void memlist_dealloc(MemoryList* list, page_t pml4) {
    list_remove(&list->list);
    memregion_drop(list->region, pml4);
    cache_dealloc(kernel.memlist_cache, list);
}
void memlist_add(struct list *list, MemoryList *new);
MemoryList* memlist_find_available(struct list *list, MemoryRegion* result, size_t minsize_pages, size_t maxsize_pages);

