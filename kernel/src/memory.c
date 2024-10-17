#include "memory.h"
#include "mem/bitmap.h"
#include "kernel.h"
paddr_t kernel_page_alloc() {
   mutex_lock(&kernel.map_lock);
   void* addr = bitmap_alloc(&kernel.map, 1);
   paddr_t res = (paddr_t)(((uintptr_t)addr) & (~KERNEL_MEMORY_MASK));
   mutex_unlock(&kernel.map_lock);
   return res;
}
void kernel_page_dealloc(paddr_t page) {
   mutex_lock(&kernel.map_lock);
   bitmap_dealloc(&kernel.map, (void*)page, 1);
   mutex_unlock(&kernel.map_lock);
}

void* kernel_malloc(size_t size) {
   mutex_lock(&kernel.map_lock);
   void* addr = bitmap_alloc(&kernel.map, (size+(PAGE_SIZE-1))/PAGE_SIZE);
   if(addr) addr = (void*)((uintptr_t)addr | KERNEL_MEMORY_MASK);
   mutex_unlock(&kernel.map_lock);
   return addr;
}
void kernel_dealloc(void* data, size_t size) {
   if(!data) return;
   mutex_lock(&kernel.map_lock);
   bitmap_dealloc(&kernel.map, (void*)((uintptr_t)data & (~KERNEL_MEMORY_MASK)),(size+(PAGE_SIZE-1))/PAGE_SIZE);
   mutex_unlock(&kernel.map_lock);
}
