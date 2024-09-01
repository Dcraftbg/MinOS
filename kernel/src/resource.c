#include "resource.h"
#include "slab.h"
static Resource* new_resource() {
    Resource* res = (Resource*)cache_alloc(kernel.resource_cache);
    if(res) memset(res, 0, sizeof(*res));
    res->shared = 1;
    return res;
}
static void resource_drop(Resource* resource) {
    if((--resource->shared) == 0) {
        cache_dealloc(kernel.resource_cache, resource);
    }
}

Resource* resource_find_by_id(ResourceBlock* first, size_t id) {
    ResourceBlock* block = first;
    while(id > RESOURCES_PER_BLOCK) {
        if(!block) return NULL;
        block = block->next;
        id -= RESOURCES_PER_BLOCK;
    }
    return block->data[id];
}

void resource_remove(ResourceBlock* first, size_t id) {
    ResourceBlock* block = first;
    while(id > RESOURCES_PER_BLOCK) {
        if(!block) return;
        block = block->next;
        id -= RESOURCES_PER_BLOCK;
    }
    resource_drop(block->data[id]);
    block->data[id] = NULL;
}
Resource* resource_add(ResourceBlock* block, size_t* id) {
    debug_assert(block);
    debug_assert(id);
    while(block->available == 0) {
        if(!block->next) { 
           block->next = new_resource_block();
           if(!block->next) return NULL;
           break;
        }
        (*id) += RESOURCES_PER_BLOCK;
        block = block->next;
    }
    for(size_t i = 0; i < RESOURCES_PER_BLOCK; ++i) {
        Resource* res = block->data[i];
        if(!block->data[i]) {
            res = (block->data[i] = new_resource());
            block->available--;
            (*id) += i;
            return res;
        }
    }
    debug_assert(false && "Unreachable!");
}

ResourceBlock* resourceblock_clone(ResourceBlock* block) {
    ResourceBlock* nblock = new_resource_block();
    if(!nblock) return NULL;
    for(size_t i = 0; i < RESOURCES_PER_BLOCK; ++i) {
        block->data[i]->shared++;
    }
    nblock->next = NULL;
    nblock->available = block->available;
    memcpy(nblock->data, block->data, sizeof(block->data));
    return nblock;
}
