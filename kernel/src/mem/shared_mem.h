#pragma once
#include <stdatomic.h>
// TODO: fragment into invidiual pages if need be.
typedef struct SharedMemory {
    atomic_size_t shared;
    paddr_t phys;
    size_t pages_count;
} SharedMemory;

void shmdrop(SharedMemory* shm);
void init_shm_cache(void);
