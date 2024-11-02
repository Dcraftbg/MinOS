#include <stdlib.h>
#include <string.h>
#include <minos/sysstd.h>
#include <strinternal.h>
void exit(int64_t code) {
    _exit(code);
    for(;;);
}
_LibcInternalHeap _libc_internal_heaps[_LIBC_INTERNAL_HEAPS_MAX];
void* malloc(size_t size) {
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        if(_libc_internal_heaps[i].id == _LIBC_INTERNAL_INVALID_HEAPID) {
            intptr_t e = heap_create(HEAP_RESIZABLE);
            if(e < 0) return NULL; // Failed to create heap. Most likely out of memory
            _libc_internal_heaps[i].id = e;
        }
        void* addr;
        if(heap_allocate(_libc_internal_heaps[i].id, size, &addr) == 0) return addr;
    }
    // Reached limit
    return NULL;
}

void* calloc(size_t elm, size_t size) {
    void* buf = malloc(elm*size);
    if(buf) {
        memset(buf, 0, elm*size);
    }
    return buf;
}
#include <stdio.h>
void* realloc(void* ptr, size_t newsize) {
    fprintf(stderr, "ERROR: Unimplemented `realloc` (%p, %zu)", ptr, newsize);
    return NULL;
}
// TODO: Smarter free with heap_get() and checking heap ranges
void free(void* addr) {
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        if(_libc_internal_heaps[i].id == _LIBC_INTERNAL_INVALID_HEAPID) continue;
        heap_deallocate(_libc_internal_heaps[i].id, addr);
    }
    // Reached limit
}
void _libc_internal_init_heap() {
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        _libc_internal_heaps[i].id = _LIBC_INTERNAL_INVALID_HEAPID;
    }
}

int atoi(const char *str) {
    const char* end;
    return atoi_internal(str, &end);
}
float atof(const char *str) {
    const char* end;
    float result=0;
    int whole = atoi_internal(str, &end);
    if(end[0] == '.') {
        size_t fract = atosz_internal(end+1, &end);
        while(fract != 0) {
            result += fract%10;
            result /= 10.0;
            fract /= 10;
        }
    }
    result += (float)whole;
    return result;
}
int abs(int num) {
    return num < 0 ? -num : num;
}
