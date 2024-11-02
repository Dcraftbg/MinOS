#pragma once
#include <stddef.h>
#include <stdint.h>
void exit(int64_t code) __attribute__ ((noreturn));
void* malloc(size_t size);
void* calloc(size_t elm, size_t size);
void* realloc(void* ptr, size_t newsize);
void free(void* addr);
void _libc_internal_init_heap();
typedef struct {
    size_t id;
} _LibcInternalHeap;
#define _LIBC_INTERNAL_HEAPS_MAX 5
#define _LIBC_INTERNAL_INVALID_HEAPID ((size_t)-1)
extern _LibcInternalHeap _libc_internal_heaps[_LIBC_INTERNAL_HEAPS_MAX];

int atoi(const char *str);
float atof(const char *str);
int abs(int num);
