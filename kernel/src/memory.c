#include "memory.h"
#include "mem/bitmap.h"
#include "kernel.h"
page_t kernel_page_alloc() {
   void* addr = bitmap_alloc(&kernel.map, 1);
   return (page_t)(((uintptr_t)addr) & (~KERNEL_MEMORY_MASK));
}
void kernel_page_dealloc(page_t page) {
   bitmap_dealloc(&kernel.map, (void*)page, 1);
}

void* kernel_malloc(size_t size) {
   void* addr = bitmap_alloc(&kernel.map, (size+(PAGE_SIZE-1))/PAGE_SIZE);
   if(addr) addr = (void*)((uintptr_t)addr | KERNEL_MEMORY_MASK);
   return addr;
}
void kernel_dealloc(void* data, size_t size) {
   if(!data) return;
   bitmap_dealloc(&kernel.map, (void*)((uintptr_t)data & (~KERNEL_MEMORY_MASK)),(size+(PAGE_SIZE-1))/PAGE_SIZE);
}
