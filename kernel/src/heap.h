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
    uintptr_t address;
    size_t pages;
} Heap;
void init_heap();
Heap* heap_new_empty(uintptr_t address, size_t pages);
Heap* heap_new(uintptr_t address, size_t pages);
void* heap_allocate(Heap* heap, size_t size);
void heap_deallocate(Heap* heap, void* addr);
Heap* heap_clone(Heap* heap);
void heap_destroy(Heap* heap);
