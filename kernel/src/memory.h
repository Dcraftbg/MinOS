#include "page.h"
paddr_t kernel_page_alloc();
void kernel_page_dealloc(paddr_t page);
void* kernel_malloc(size_t size);
void  kernel_dealloc(void* data, size_t size);

