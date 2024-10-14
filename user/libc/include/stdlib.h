#pragma once
#include <stddef.h>
void* malloc(size_t size);
void free(void* addr);
void* realloc(void* addr, size_t size);
void _libc_internal_init_heap();
typedef struct {
    size_t id;
} _LibcInternalHeap;
#define _LIBC_INTERNAL_HEAPS_MAX 5
#define _LIBC_INTERNAL_INVALID_HEAPID ((size_t)-1)
extern _LibcInternalHeap _libc_internal_heaps[_LIBC_INTERNAL_HEAPS_MAX];
