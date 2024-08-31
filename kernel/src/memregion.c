#include "memregion.h"

void init_memregion() {
    assert(kernel.memregion_cache=create_new_cache(sizeof(MemoryRegion), "MemoryRegion")); 
    assert(kernel.memlist_cache  =create_new_cache(sizeof(MemoryList  ), "MemoryList"  )); 
}
MemoryRegion* memregion_clone(MemoryRegion* region, page_t src, page_t dst) {
    if(region->flags & MEMREG_WRITE) {
        uintptr_t at = region->address;
        for(size_t i = 0; i < region->pages; ++i) {
            if(!page_alloc(dst, at, 1, region->pageflags)) return NULL;
                  void* taddr = (      void*)(virt_to_phys(dst, at) | KERNEL_MEMORY_MASK);
            const void* saddr = (const void*)(virt_to_phys(src, at) | KERNEL_MEMORY_MASK);
            memcpy(taddr, saddr, PAGE_SIZE);
        }
        region->shared++;
        return region;
    } 
    region->shared++;
    return region;
}

