#include "heap.h"
void init_heap() {
    // TODO: maybe every task should have its own cache for these
    assert(kernel.allocation_cache=create_new_cache(sizeof(Allocation), "Allocation"));
    assert(kernel.heap_cache=create_new_cache(sizeof(Heap), "Heap")); 
}
static Allocation* allocation_new(bool free, size_t size, size_t offset) {
    Allocation* allocation = (Allocation*)cache_alloc(kernel.allocation_cache);
    if(allocation) {
        list_init(&allocation->list);
        allocation->free = free;
        allocation->size = size;
        allocation->offset = offset;
    }
    return allocation;
}
Heap* heap_new(MemoryRegion* region) {
    Heap* heap = (Heap*)cache_alloc(kernel.heap_cache);
    if(!heap) return NULL;
    Allocation* allocation = allocation_new(true, region->pages*PAGE_SIZE, region->address);
    if(!allocation) {
        cache_dealloc(kernel.heap_cache, heap);
        return NULL;
    }
    list_init(&heap->list);
    list_init(&heap->allocation_list);
    list_append(&allocation->list, &heap->allocation_list);
    heap->region = region;
    return heap;
}
static Allocation* allocation_split(Allocation* alloc, size_t size) {
    Allocation* new = allocation_new(true, alloc->size-size, alloc->offset+size);
    if(!new) return NULL;
    alloc->free   = false;
    alloc->size   = size;
    list_append(&new->list, &alloc->list);
    return new;
}
// TODO: Lock
void* heap_allocate(Heap* heap, size_t size) {
    Allocation* alloc = (Allocation*)heap->allocation_list.next;
    while(&alloc->list != &heap->allocation_list) {
        if(alloc->free) {
            if(alloc->size > size) {
                if(allocation_split(alloc, size)) return (void*)(heap->region->address+alloc->offset);
            } else if(alloc->size == size) {
                alloc->free = false;
                return (void*)(heap->region->address+alloc->offset);
            }
        }
    }
    return NULL;
}
// TODO: Lock
void heap_deallocate(Heap* heap, void* addr) {
    if((void*)heap->region->address < addr || addr > (void*)(heap->region->address+heap->region->pages*PAGE_SIZE)) // Out of range
        return;
    Allocation* allocation = (Allocation*)heap->allocation_list.next;
    size_t off = (size_t)(addr-heap->region->address);
    while(&allocation->list != &heap->allocation_list) {
        if(allocation->offset == off) {
            if(allocation->free) return; // Double free
            allocation->free = true;
            if(allocation->list.next != &heap->allocation_list && ((Allocation*)allocation->list.next)->free) {
                Allocation* next = (Allocation*)allocation->list.next;
                allocation->size = allocation->size + next->size;
                list_remove(&next->list);
                cache_dealloc(kernel.allocation_cache, next);
            }
            return;
        }
    }
}
