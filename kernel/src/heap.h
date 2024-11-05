#pragma once
#include "mem/memregion.h"
#include <collections/list.h>
#include <stddef.h>
#include <stdatomic.h>
typedef struct {
    struct list list;
    struct list allocation_list;
    size_t id;
    uint64_t flags;
    uintptr_t address;
    size_t pages;
} Heap;
void init_heap();
Heap* heap_new(size_t id, uintptr_t address, size_t pages, uint64_t flags);
intptr_t heap_extend(Heap* heap, size_t extra);
Heap* heap_clone(Heap* heap);
void heap_destroy(Heap* heap);
