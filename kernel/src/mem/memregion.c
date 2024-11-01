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
    if(!page_share(src, dst, region->address, region->pages)) return NULL;
    region->shared++;
    return region;
}

void memlist_add(struct list *list, MemoryList *new) {
    assert(new && new->region);
    MemoryList* head = (MemoryList*)list->next;
    while(&head->list != list) {
        uintptr_t next_addr = 0xffffffffffffffff;
        if(head->list.next != list) {
            next_addr = ((MemoryList*)head->list.next)->region->address;
        }
        if(head->region->address < new->region->address && new->region->address < next_addr) {
            list_append(&new->list, &head->list);
            return;
        }
        head = (MemoryList*)head->list.next;
    }
    list_append(&new->list, &head->list);
}
MemoryList* memlist_find_available(struct list *list, MemoryRegion* result, size_t minsize_pages, size_t maxsize_pages) {
    MemoryList* head = (MemoryList*)list->next;
    // TODO: Allow allocation before the first chunk of memory
    while(&head->list != list) {
        uintptr_t next_addr = 0xffffffffffffffffLL;
        if(head->list.next != list) {
            next_addr = ((MemoryList*)head->list.next)->region->address;
        }
        size_t size_bytes = PAGE_SIZE*head->region->pages;
        if(head->region->address + size_bytes < next_addr) {
            result->address = head->region->address + size_bytes;
            result->pages   = (next_addr-result->address)/PAGE_SIZE;
            result->pages   = result->pages > maxsize_pages ? maxsize_pages : result->pages;
            if(result->pages >= minsize_pages) return head;
        }
        head = (MemoryList*)head->list.next;
    }
    return NULL;
}

// TODO: Binary search
MemoryList* memlist_find(struct list *list, void* address) {
    MemoryList* head = (MemoryList*)list->next;
    while(&head->list != list) {
        if((void*)head->region->address > address) return NULL;
        if((void*)head->region->address == address) return head;
        head = (MemoryList*)head->list.next;
    }
    return NULL;
}
