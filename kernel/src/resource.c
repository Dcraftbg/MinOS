#include "resource.h"
#include "slab.h"
static Resource* new_resource() {
    Resource* res = (Resource*)cache_alloc(kernel.resource_cache);
    if(res) memset(res, 0, sizeof(*res));
    return res;
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

