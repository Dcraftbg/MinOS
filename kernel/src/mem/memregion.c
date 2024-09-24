#include "memregion.h"

void init_memregion() {
    assert(kernel.memregion_cache=create_new_cache(sizeof(MemoryRegion), "MemoryRegion")); 
    assert(kernel.memlist_cache  =create_new_cache(sizeof(MemoryList  ), "MemoryList"  )); 
}
MemoryRegion* memregion_clone(MemoryRegion* region, page_t src, page_t dst) {
    // Read-Write
    if(region->flags & MEMREG_WRITE) {
        uintptr_t at = region->address;
        for(size_t i = 0; i < region->pages; ++i) {
            if(!page_alloc(dst, at, 1, region->pageflags)) return NULL;
                  void* tphys = (      void*)(virt_to_phys(dst, at));
            assert(tphys);
                  void* taddr = (      void*)((uintptr_t)tphys | KERNEL_MEMORY_MASK);
            const void* sphys = (const void*)(virt_to_phys(src, at));
            assert(sphys);
            const void* saddr = (const void*)((uintptr_t)sphys | KERNEL_MEMORY_MASK);
            memcpy(taddr, saddr, PAGE_SIZE);
            at+=PAGE_SIZE;
        }
        region->shared++;
        return region;
    }
    // Read only
    uintptr_t at = region->address;
    for(size_t i = 0; i < region->pages; ++i) {
        uintptr_t sphys = virt_to_phys(src, at);
        assert(sphys);
        // TODO: page_copy_map ?
        if(!page_mmap(dst, sphys, at, 1, region->pageflags)) return NULL;
        at+=PAGE_SIZE;
    }
    region->shared++;
    return region;
}

