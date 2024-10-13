#include "heap.h"
#include "kernel.h"
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

Heap* heap_new_empty(size_t id, uintptr_t address, size_t pages) {
    Heap* heap = (Heap*)cache_alloc(kernel.heap_cache);
    if(!heap) return NULL;
    list_init(&heap->list);
    list_init(&heap->allocation_list);
    heap->id = id;
    heap->address = address;
    heap->pages = pages;
    return heap;
}
Heap* heap_new(size_t id, uintptr_t address, size_t pages) {
    Heap* heap = heap_new_empty(id, address, pages);
    if(!heap) return NULL;
    Allocation* allocation = allocation_new(true, heap->pages*PAGE_SIZE, heap->address);
    if(!allocation) {
        heap_destroy(heap);
        return NULL;
    }
    list_append(&allocation->list, &heap->allocation_list);
    return heap;
}
static Allocation* allocation_clone_shallow(Allocation* alloc) {
    return allocation_new(alloc->free, alloc->size, alloc->offset);
}
void allocation_destroy_shallow(Allocation* alloc) {
    list_remove(&alloc->list);
    cache_dealloc(kernel.allocation_cache, alloc);
}
void heap_destroy(Heap* heap) {
    list_remove(&heap->list);
    Allocation* alloc = (Allocation*)heap->allocation_list.next;
    while(&alloc->list != &heap->allocation_list) {
        Allocation* next_alloc = (Allocation*)alloc->list.next;
        allocation_destroy_shallow(alloc);
        alloc = next_alloc;
    }
    cache_dealloc(kernel.heap_cache, heap);
}
Heap* heap_clone(Heap* heap) {
    Heap* new = heap_new_empty(heap->id, heap->address, heap->pages);
    if(!new) return NULL;
    Allocation* alloc = (Allocation*)heap->allocation_list.next;
    while(&alloc->list != &heap->allocation_list) {
        Allocation* new_alloc = allocation_clone_shallow(alloc);
        if(!new_alloc) {
            heap_destroy(new);
            return NULL;
        }
        list_append(&new_alloc->list, new->list.prev);
        alloc = (Allocation*)alloc->list.next;
    }
    return new;
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
                if(allocation_split(alloc, size)) return (void*)(heap->address+alloc->offset);
            } else if(alloc->size == size) {
                alloc->free = false;
                return (void*)(heap->address+alloc->offset);
            }
        }
    }
    return NULL;
}
// TODO: Lock
void heap_deallocate(Heap* heap, void* addr) {
    if((void*)heap->address < addr || addr > (void*)(heap->address+heap->pages*PAGE_SIZE)) // Out of range
        return;
    Allocation* allocation = (Allocation*)heap->allocation_list.next;
    size_t off = (size_t)(addr-heap->address);
    while(&allocation->list != &heap->allocation_list) {
        if(allocation->offset == off) {
            if(allocation->free) return; // Double free
            allocation->free = true;
            if(allocation->list.next != &heap->allocation_list && ((Allocation*)allocation->list.next)->free) {
                Allocation* next = (Allocation*)allocation->list.next;
                allocation->size = allocation->size + next->size;
                allocation_destroy_shallow(next);
            }
            return;
        }
    }
}
