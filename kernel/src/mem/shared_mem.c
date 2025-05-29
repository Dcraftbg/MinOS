#include "kernel.h"
#include "log.h"
#include "slab.h"
#include "shared_mem.h"

void init_shm_cache(void) {
    if(!(kernel.shared_memory_cache = create_new_cache(sizeof(SharedMemory), "SharedMemory"))) kpanic("Not enough memory for shared_memory_cache");
}

void shmdrop(SharedMemory* shm) {
    if(shm->shared--) return;
    kernel_dealloc((void*)(shm->phys + KERNEL_MEMORY_MASK), PAGE_SIZE*shm->pages_count);
    cache_dealloc(kernel.shared_memory_cache, shm);
}
