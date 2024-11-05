#include "heap.h"
#include "kernel.h"
void init_heap() {
    assert(kernel.heap_cache=create_new_cache(sizeof(Heap), "Heap")); 
}
Heap* heap_new(size_t id, uintptr_t address, size_t pages, uint64_t flags) {
    Heap* heap = (Heap*)cache_alloc(kernel.heap_cache);
    if(!heap) return NULL;
    list_init(&heap->list);
    heap->id = id;
    heap->address = address;
    heap->pages = pages;
    heap->flags = flags;
    return heap;
}
void heap_destroy(Heap* heap) {
    list_remove(&heap->list);
    cache_dealloc(kernel.heap_cache, heap);
}
Heap* heap_clone(Heap* heap) {
    return heap_new(heap->id, heap->address, heap->pages, heap->flags);
}
intptr_t heap_extend(Heap* heap, size_t extra) {
    heap->pages += extra;
    return 0;
}
