#include "iomem.h"
#include "kernel.h"
void* iomap(paddr_t phys, size_t pages, pageflags_t flags) {
    uintptr_t at = page_find_available(kernel.pml4, REGION_IO_SPACE_ADDR, pages, REGION_IO_SPACE_PAGES);
    if(!at) return NULL;
    page_mmap(kernel.pml4, phys, at, pages, flags);
    invalidate_pages((void*)at, pages);
    return (void*)at;
}
void iounmap(void* at, size_t pages) {
    if(!at) return;
    page_unmap(kernel.pml4, (uintptr_t)at, pages);
}

void* iomap_bytes(paddr_t at, size_t size, pageflags_t flags) {
    size_t pages = (PAGE_ALIGN_UP(at+size)-PAGE_ALIGN_DOWN(at))/PAGE_SIZE;
    return iomap(PAGE_ALIGN_DOWN(at), pages, flags) + at%PAGE_SIZE;
}
void iounmap_bytes(void* at, size_t size) {
    size_t pages = (PAGE_ALIGN_UP((uintptr_t)at+size)-PAGE_ALIGN_DOWN((uintptr_t)at))/PAGE_SIZE;
    iounmap(at, pages);
}
