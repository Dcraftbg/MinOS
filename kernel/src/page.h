#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "utils.h"
#define KERNEL_PAGE_ENTRIES 512
#define KERNEL_PAGE_ENTRY_SIZE sizeof(uint64_t)
#include "vmm.h"


#define KERNEL_PTYPE_SHIFT 9
#define KERNEL_PFLAG_PRESENT       0b1
#define KERNEL_PFLAG_WRITE         0b10
#define KERNEL_PFLAG_USER          0b100
#define KERNEL_PFLAG_WRITE_THROUGH 0b1000
#define KERNEL_PFLAG_WRITE_COMBINE 0b10000000
#define KERNEL_PFLAG_CACHE_DISABLE 0b10000
#define KERNEL_PFLAG_EXEC_DISABLE  0// 0b1000000000000000000000000000000000000000000000000000000000000000
// NOTE: Dirty
#define KERNEL_PFLAG_ACCESSED      0b100000

#define KENREL_PTYPE_MASK 0b111000000000

#define KERNEL_PTYPE_KERNEL  (0b0   << KERNEL_PTYPE_SHIFT)
#define KERNEL_PTYPE_USER    (0b111 << KERNEL_PTYPE_SHIFT)

#define KERNEL_PFLAGS_MASK 0b1000000000000000000000000000000000000000000000000000111111111111LL
// Technically incorrect but idrk
#define KERNEL_PADDR_MASK  0b0111111111111111111111111111111111111111111111111111000000000000LL 

typedef uint64_t *page_t;
typedef uint64_t pageflags_t;
typedef uintptr_t paddr_t;
bool page_mmap(page_t pml4_addr, uintptr_t phys, uintptr_t virt, size_t pages_count, pageflags_t flags);
bool page_alloc(page_t pml4_addr, uintptr_t virt, size_t pages_count, pageflags_t flags);
bool page_share(page_t parent, page_t child, uintptr_t virt, size_t pages_count);
void page_join(page_t parent, page_t child);
void page_unmap(page_t pml4_addr, uintptr_t virt, size_t pages_count);
void page_unalloc(page_t pml4_addr, uintptr_t virt, size_t pages_count);
void page_destruct(page_t pml4, uint16_t type);
uintptr_t page_find_available(page_t pml4, uintptr_t from, size_t pages, size_t max);
uintptr_t virt_to_phys(page_t pml4_addr, uintptr_t addr);
void init_paging(); // Called to initialse

#define KERNEL_SWITCH_VTABLE() \
    __asm__ volatile (\
       "movq %0, %%cr3\n"\
       "movq %1, %%rsp\n"\
       "movq $0, %%rbp\n"\
       :\
       : "r" ((uintptr_t)kernel.pml4 & ~KERNEL_MEMORY_MASK), \
         "r" (KERNEL_STACK_PTR)\
    )

// debug functions
void page_flags_serialise(pageflags_t flags, char* buf, size_t cap);
const char* page_type_str(pageflags_t flags);

// Invalidating stuff
extern void invalidate_full_page_table();
void invalidate_pages(void* address, size_t pages);
