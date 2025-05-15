#pragma once
#include "vfs.h"
#include "kernel.h"
#include <stdint.h>
#include "assert.h"
#include "string.h"
#include "mem/slab.h"
#include <stdatomic.h>
#include "epoll.h"
#include "socket.h"
#include <minos/fsdefs.h>
typedef struct {
    oflags_t flags;
    off_t offset;
    Inode* inode;
} Resource;
#define RESOURCES_PER_BLOCK 1022
typedef struct ResourceBlock {
    struct ResourceBlock* next;
    size_t occupied;
    Resource* data[RESOURCES_PER_BLOCK];
} ResourceBlock;
static ResourceBlock* new_resource_block() {
    ResourceBlock* block = (ResourceBlock*)kernel_malloc(sizeof(*block));
    if(block) memset(block, 0, sizeof(*block));
    return block;
}
void resourceblock_dealloc(ResourceBlock* block);
Resource* resource_add(ResourceBlock* block, size_t* id);
Resource* resource_find_by_id(ResourceBlock* first, size_t id);
void resource_remove(ResourceBlock* first, size_t id);
ResourceBlock* resourceblock_clone(ResourceBlock* block);
static void init_resources() {
    assert(kernel.resource_cache = create_new_cache(sizeof(Resource), "Resources"));
}
