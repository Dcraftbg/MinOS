#pragma once
#include "page.h"

paddr_t kernel_pages_alloc(size_t count);
paddr_t kernel_page_alloc();
void kernel_page_dealloc(paddr_t page);
void kernel_pages_dealloc(paddr_t page, size_t count);
void* kernel_malloc(size_t size);
void  kernel_dealloc(void* data, size_t size);

