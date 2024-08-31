#include "memregion.h"

void init_memregion() {
    assert(kernel.memregion_cache=create_new_cache(sizeof(MemoryRegion), "MemoryRegion")); 
    assert(kernel.memlist_cache  =create_new_cache(sizeof(MemoryList  ), "MemoryList"  )); 
}
