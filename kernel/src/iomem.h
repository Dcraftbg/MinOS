#pragma once
#include "page.h"
void* iomap(paddr_t phys, size_t pages, pageflags_t flags);
void iounmap(void* at, size_t pages);
void* iomap_bytes(paddr_t at, size_t size, pageflags_t flags);
void iounmap_bytes(void* at, size_t size);
