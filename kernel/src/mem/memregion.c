#include "memregion.h"
#include "kernel.h"


void init_memregion() {
    assert(kernel.memregion_cache=create_new_cache(sizeof(MemoryRegion), "MemoryRegion")); 
    assert(kernel.memlist_cache  =create_new_cache(sizeof(MemoryList  ), "MemoryList"  )); 
}
MemoryRegion* memregion_clone(MemoryRegion* region, page_t src, page_t dst) {
    // Read-Write
    if(region->pageflags & KERNEL_PFLAG_WRITE) {
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

void memlist_add(struct list_head *list, MemoryList *new) {
    assert(new && new->region);
    MemoryList* head = (MemoryList*)list->next;
    while(&head->list != list) {
        uintptr_t next_addr = 0xffffffffffffffff;
        if(head->list.next != list) {
            next_addr = ((MemoryList*)head->list.next)->region->address;
        }
        if(head->region->address < new->region->address && new->region->address < next_addr) {
            list_append(&head->list, &new->list);
            return;
        }
        head = (MemoryList*)head->list.next;
    }
    list_append(&head->list, &new->list);
}
MemoryList* memlist_find_available(struct list_head *list, MemoryRegion* result, void* post_addr, size_t minsize_pages, size_t maxsize_pages) {
    MemoryList* head = (MemoryList*)list;
    do {
        uintptr_t next_addr = 0xffffffffffffffffLL;
        if(head->list.next != list) {
            next_addr = ((MemoryList*)head->list.next)->region->address;
        }
        uintptr_t addr = 0;
        size_t size_bytes = 0;
        if(&head->list != list) {
            addr = head->region->address;
            size_bytes = PAGE_SIZE*head->region->pages;
        }
        if(addr + size_bytes < (uintptr_t)post_addr && (uintptr_t)post_addr > next_addr) {
            head = (MemoryList*)head->list.next;
            continue;
        }
        if(addr + size_bytes < next_addr) {
            result->address = addr + size_bytes;
            result->pages   = (next_addr-result->address)/PAGE_SIZE;
            result->pages   = result->pages > maxsize_pages ? maxsize_pages : result->pages;
            if(result->pages >= minsize_pages) return head;
        }
        head = (MemoryList*)head->list.next;
    } while(&head->list != list); 
    return NULL;
}

// TODO: Binary search
MemoryList* memlist_find(struct list_head *list, void* address) {
    MemoryList* head = (MemoryList*)list->next;
    while(&head->list != list) {
        if((void*)head->region->address > address) return NULL;
        if((void*)head->region->address == address) return head;
        head = (MemoryList*)head->list.next;
    }
    return NULL;
}
MemoryRegion* memregion_new(pageflags_t pageflags, uintptr_t address, size_t pages) {
    MemoryRegion* region;
    if(!(region=cache_alloc(kernel.memregion_cache))) return NULL;
    region->pageflags = pageflags;
    region->address = address;
    region->pages = pages;
    region->shared = 1;
    return region;
}
void memlist_dealloc(MemoryList* list, page_t pml4) {
    list_remove(&list->list);
    memregion_drop(list->region, pml4);
    cache_dealloc(kernel.memlist_cache, list);
}
MemoryList* memlist_new(MemoryRegion* region) {
    if(!region) return NULL;
    MemoryList* list = (MemoryList*)cache_alloc(kernel.memlist_cache);
    if(!list) return NULL;
    list->region = region;
    list_init(&list->list);
    return list;
}

void memregion_drop(MemoryRegion* region, page_t pml4) {
    if(region->shared == 1) {
        // if(pml4) page_dealloc(pml4, region->address, region->pages);
        cache_dealloc(kernel.memregion_cache, region);
        return;
    }
    region->shared--;
}
