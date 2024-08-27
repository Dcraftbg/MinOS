#include "slab.h"
#include <stdbool.h>
#include "kernel.h"
#include "string.h"
static size_t slab_mem(Cache* cache) {
    return cache->objs_per_slab * (cache->objsize + sizeof(uint32_t)) + sizeof(Slab);
}
// TODO: Flags for on/off slab
// https://github.com/torvalds/linux/blob/b1cb0982bdd6f57fed690f796659733350bb2cae/mm/slab.c#L2575
intptr_t cache_grow(Cache* cache) {
    void *addr = kernel_malloc(slab_mem(cache));
    if(!addr) return -1; // @STATUS 
    Slab* slab = addr;
    slab->cache = cache;
    // Memory follows immediately after the slab 
    // (we have the memory field because in the future
    //  you might decide to move the memory to its own page)
    slab->memory = ((uint32_t*)(addr+sizeof(*slab)))+cache->objs_per_slab;
    slab->free = 0;
    cache->totalobjs+=cache->objs_per_slab;
    for(size_t i = 0; i < cache->objs_per_slab-1; ++i) {
        slab_bufctl(slab)[i] = i;
    }
    list_append((struct list*)slab, &cache->free);
    return 0;
}
// Selects a cache in which you can allocate your objects
static Slab* cache_select(Cache* cache) {
    Slab* s = NULL;
    if((s = (Slab*)list_next(&cache->partial))) return s;
    if((s = (Slab*)list_next(&cache->free))) return s;
    if(cache_grow(cache) != 0) return NULL; // @STATUS
    s = (Slab*)list_next(&cache->free);
    return s;
}
void* cache_alloc(Cache* cache) {
    debug_assert(cache);
    Slab* slab = cache_select(cache);
    if(!slab) return NULL;
    size_t index = slab_bufctl(slab)[slab->free];
    void* ptr = slab->memory + cache->objsize * index;
    // The Slab was free before, lets move it to partial since we allocated something
    if(slab->free++ == 0) {
        list_remove(&slab->list);
        list_append(&cache->partial, &slab->list);
    }
    // The Slab was paritally filled, but now is full, lets move it to the full linked list
    if(slab->free == cache->objs_per_slab) {
        list_remove(&slab->list);
        list_append(&cache->full, &slab->list);
    }
    cache->inuse++;
    return ptr;
}
static size_t slab_ptr_to_index(Cache* cache, Slab* slab, void* p) {
    return (size_t)(p - slab->memory) / cache->objsize;
}
static bool cache_dealloc_within(Cache* cache, Slab* slab, void* p) {
    Slab *first = slab;
    while(slab != (Slab*)first->list.prev) {
        if(p >= slab->memory && p < slab->memory+cache->objsize*cache->objs_per_slab) {
            assert(slab->free);
            if(slab->free-- == cache->objs_per_slab) {
                list_remove(&slab->list);
                list_append(&cache->partial, &slab->list);
            }
            if(slab->free == 0) {
                list_remove(&slab->list);
                list_append(&cache->free, &slab->list);
            }
            size_t index = slab_ptr_to_index(cache, slab, p);
            slab_bufctl(slab)[slab->free]=index;
            cache->inuse--;
            return true;
        }
        slab = (Slab*)slab->list.next;
    }
    return false;
}
void cache_dealloc(Cache* cache, void* p) {
    debug_assert(cache);
    debug_assert(p);
    Slab *s = (Slab*)list_next(&cache->full);
    if(s) {
        if(cache_dealloc_within(cache, s, p)) return;
    }
    s = (Slab*)list_next(&cache->partial);
    if(s) {
        if(cache_dealloc_within(cache, s, p)) return;
    }
}
// TODO: Non-gready algorithmn for better optimisations 
// in memory usage AND that takes in consideration the Slab itself
// And of course the bufctl index stack
static void init_cache(Cache* c, size_t objsize) {
    memset(c, 0, sizeof(*c)); 
    list_init(&c->list);
    list_init(&c->partial);
    list_init(&c->full);
    list_init(&c->free);
    c->objsize = objsize;
    // Less Gready algorithmn
    c->objs_per_slab = (PAGE_ALIGN_UP(objsize+sizeof(Slab))-sizeof(Slab)) / (objsize+sizeof(uint32_t));
    // c->objs_per_slab = PAGE_ALIGN_UP(objsize) / (objsize);
}
Cache* create_new_cache(size_t objsize, const char* name) {
    size_t len = strlen(name);
    if(len >= MAX_CACHE_NAME) return NULL;
    assert(kernel.cache_cache); // @DEBUG      Ensures correct usage
    Cache* c = (Cache*)cache_alloc(kernel.cache_cache);
    if(!c) return NULL;
    init_cache(c, objsize);
    memcpy(c->name, name, len+1);
    list_append(&c->list, &kernel.cache_list);
    return c;
}
void init_cache_cache() {
    kernel.cache_cache = kernel_malloc(sizeof(Cache));
    init_cache(kernel.cache_cache, sizeof(Cache));
    list_init(&kernel.cache_list);
}
