#include <stdlib.h>
#include <string.h>
#include <minos/sysstd.h>

_LibcInternalHeap _libc_internal_heaps[_LIBC_INTERNAL_HEAPS_MAX];
void* malloc(size_t size) {
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        if(_libc_internal_heaps[i].id == _LIBC_INTERNAL_INVALID_HEAPID) {
            intptr_t e = heap_create();
            if(e < 0) return NULL; // Failed to create heap. Most likely out of memory
            _libc_internal_heaps[i].id = e;
        }
        void* addr;
        if(heap_allocate(_libc_internal_heaps[i].id, size, &addr) == 0) return addr;
    }
    // Reached limit
    return NULL;
}

void* realloc(void* addr, size_t size) {
    void* nbuf = malloc(size);
    if(!nbuf) return NULL;
    memcpy(nbuf, addr, size);
    free(addr);
    return nbuf;
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
