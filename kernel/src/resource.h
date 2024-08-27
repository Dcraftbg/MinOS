#pragma once
#include "vfs.h"
#include "kernel.h"
#include <stdint.h>
#include "assert.h"
#include "string.h"
#include "slab.h"
enum {
   RESOURCE_FILE=1
};
typedef uint32_t resourcekind_t;
typedef struct {
   resourcekind_t kind;
   union {
       VfsFile file;
   } data;
} Resource;
#define RESOURCES_PER_BLOCK 1022
typedef struct ResourceBlock {
   struct ResourceBlock* next;
   uintptr_t available;
   Resource* data[RESOURCES_PER_BLOCK];
} ResourceBlock;
static ResourceBlock* new_resource_block() {
   ResourceBlock* block = (ResourceBlock*)kernel_malloc(sizeof(*block));
   if(block) memset(block, 0, sizeof(*block));
   return block;
}
static void delete_resource_block(ResourceBlock* block) {
   kernel_dealloc(block, sizeof(*block));
}
Resource* resource_add(ResourceBlock* block, size_t* id);
Resource* resource_find_by_id(ResourceBlock* first, size_t id);
void resource_remove(ResourceBlock* first, size_t id);
static void init_resources() {
   assert(kernel.resource_cache = create_new_cache(sizeof(Resource), "Resources"));
}
