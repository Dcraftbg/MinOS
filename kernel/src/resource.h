#pragma once
#include <stdint.h>
#include <stddef.h>
#include <minos/fsdefs.h>
typedef struct Inode Inode;
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
ResourceBlock* new_resource_block(void);
void resourceblock_dealloc(ResourceBlock* block);
Resource* resource_add(ResourceBlock* block, size_t* id);
Resource* resource_find_by_id(ResourceBlock* first, size_t id);
void resource_remove(ResourceBlock* first, size_t id);
ResourceBlock* resourceblock_clone(ResourceBlock* block);
void init_resources(void);
