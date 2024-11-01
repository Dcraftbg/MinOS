#pragma once
#include "mem/memregion.h"
#include "list.h"
#include <stddef.h>
#include <stdatomic.h>
// TODO: There is no need for offset since allocations are placed one after the other
typedef struct {
    struct list list;
    bool free;
    size_t size, offset;
} Allocation;
typedef struct {
    struct list list;
    struct list allocation_list;
    size_t id;
    uint64_t flags;
    uintptr_t address;
    size_t pages;
} Heap;
void init_heap();
Heap* heap_new_empty(size_t id, uintptr_t address, size_t pages, uint64_t flags);
Heap* heap_new(size_t id, uintptr_t address, size_t pages, uint64_t flags);
void* heap_allocate(Heap* heap, size_t size);
intptr_t heap_extend(Heap* heap, size_t extra);
void heap_deallocate(Heap* heap, void* addr);
Heap* heap_clone(Heap* heap);
void heap_destroy(Heap* heap);
