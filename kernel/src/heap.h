#pragma once
#include "mem/memregion.h"
#include "list.h"
#include "kernel.h"
// TODO: There is no need for offset since allocations are placed one after the other
typedef struct {
    struct list list;
    bool free;
    size_t size, offset;
} Allocation;
typedef struct {
    struct list list;
    struct list allocation_list;
    MemoryRegion* region;
} Heap;
void init_heap();
Heap* heap_new(MemoryRegion* region);
void* heap_allocate(Heap* heap, size_t size);
void heap_deallocate(Heap* heap, void* addr);
