#pragma once
#include "utils.h"
#include <stddef.h>
#include <stdint.h>
#include "list.h"
#define MAX_CACHE_NAME 20
// TODO: Deallocation of caches and cache_destory, cache_shrink etc.
// TODO: Give caches a name field like what linux has for @STAT purposes
typedef struct Cache {
    struct list list;
    // Linked list to either:
    // - fully filled blocks     => blocks with no available memory
    // - partially filled blocks => blocks with partially available memory
    // - free blocks             => empty blocks with fully available memory
    struct list full, partial, free;
    // @STAT
    // Total amount of objects
    size_t totalobjs;
    // @STAT
    // Amount of objects allocated currently
    size_t inuse;
    // Size of a single object
    size_t objsize;
    // Objects per slab
    size_t objs_per_slab;
    char name[MAX_CACHE_NAME];
} Cache;
typedef struct Slab { 
    struct list list;
    Cache* cache; // Cache it belongs to
    void* memory;
    // Maintain a stack of free objects (indexes to objects within the slab). Top of the stack is at free
    // We push objects and increment the free pointer when we want to allocate something,
    // and we decrement the pointer and write the free address at that point
    size_t free; 
} Slab;

// Skip the slabs' data (+1, equivilent to +sizeof(Slab))
// and right after it you'll find an array of bufctl
// aka the stack of free objects
#define slab_bufctl(slab) ((uint32_t*)(slab+1))
intptr_t cache_grow(Cache* cache);
void* cache_alloc(Cache* cache);
void cache_dealloc(Cache* cache, void* p);
Cache* create_new_cache(size_t objsize, const char* name);
void init_cache_cache();

