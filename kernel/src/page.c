#include "page.h"
#include "memory.h"
#include "kernel.h"
#include "elf.h"
#include "string.h"
#include "log.h"
#define PAGE_MASK 0xFFFF000000000000

uintptr_t page_find_available(page_t pml4_addr, uintptr_t from, size_t pages, size_t max) {
    if(pages > max) return 0;
    if(pages == 0 || max == 0) return 0;
    uint16_t pml1 = (from >> (12   )) & 0x1ff;
    uint16_t pml2 = (from >> (12+9 )) & 0x1ff;
    uint16_t pml3 = (from >> (12+18)) & 0x1ff;
    uint16_t pml4 = (from >> (12+27)) & 0x1ff;
    uintptr_t start = from;
    size_t left = pages;
    for(; pml4 < KERNEL_PAGE_ENTRIES; pml4++) {
        if(!pml4_addr[pml4]) continue;
        page_t pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4] | KERNEL_MEMORY_MASK);
        for(; pml3 < KERNEL_PAGE_ENTRIES; pml3++) {
            if(!pml3_addr[pml3]) continue;
            page_t pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3] | KERNEL_MEMORY_MASK);
            for(; pml2 < KERNEL_PAGE_ENTRIES; pml2++) {
                if(!pml2_addr[pml2]) continue;
                page_t pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2] | KERNEL_MEMORY_MASK);
                for(; pml1 < KERNEL_PAGE_ENTRIES; pml1++) {
                    if(pml1_addr[pml1]) {
                        left = pages;
                        start = 
                            (from & PAGE_MASK)
                            | ((((uintptr_t)pml4  ) & 0x1ff) << (12+27))
                            | ((((uintptr_t)pml3  ) & 0x1ff) << (12+18))
                            | ((((uintptr_t)pml2  ) & 0x1ff) << (12+9 ))
                            | ((((uintptr_t)pml1+1) & 0x1ff) << (12   ));
                        goto end;
                    }
                    if(--left == 0) return start;
                end:
                    if(--max == 0) return 0;
                }
                pml1 = 0;
            }
            pml2 = 0;
        }
        pml3 = 0;
    }
    return 0;
}
// TODO: Fix XD. XD may not be supported always so checks to remove it are necessary
bool page_mmap(page_t pml4_addr, uintptr_t phys, uintptr_t virt, size_t pages_count, pageflags_t flags) {
    // printf("Called page_mmap(/*pml4*/ 0x%p, /*phys*/ 0x%p, /*virt*/ 0x%p, /*pages*/ %zu, /*flags*/, %02X)\n",pml4_addr, phys,virt,pages_count,flags);
    virt &= ~PAGE_MASK;          // Clean up the top bits (reasons I won't get into)
    phys &= ~KERNEL_MEMORY_MASK; // Bring to a physical address just in case
    uint16_t pml1 = (virt >> (12   )) & 0x1ff;
    uint16_t pml2 = (virt >> (12+9 )) & 0x1ff;
    uint16_t pml3 = (virt >> (12+18)) & 0x1ff;
    uint16_t pml4 = (virt >> (12+27)) & 0x1ff;
    for(; pml4 < KERNEL_PAGE_ENTRIES; pml4++) {
        page_t pml3_addr = NULL;
        if(pml4_addr[pml4] == 0) {
            pml4_addr[pml4] = (uintptr_t)kernel_page_alloc();
            if(!pml4_addr[pml4]) return false; // Out of memory
            pml3_addr = (page_t)(pml4_addr[pml4] | KERNEL_MEMORY_MASK); 
            memset(pml3_addr, 0, PAGE_SIZE);
            pml4_addr[pml4] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | (flags & KERNEL_PFLAG_USER);
        } else {
            pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4] | KERNEL_MEMORY_MASK);
        }

        for(; pml3 < KERNEL_PAGE_ENTRIES; pml3++) {
            page_t pml2_addr = NULL;
            if(pml3_addr[pml3] == 0) {
                pml3_addr[pml3] = (uintptr_t)kernel_page_alloc();
                if(!pml3_addr[pml3]) return false; // Out of memory
                pml2_addr = (page_t)(pml3_addr[pml3] | KERNEL_MEMORY_MASK); 
                memset(pml2_addr, 0, PAGE_SIZE);
                pml3_addr[pml3] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | (flags & KERNEL_PFLAG_USER);
            } else {
                pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3] | KERNEL_MEMORY_MASK);
            }

            for(; pml2 < KERNEL_PAGE_ENTRIES; pml2++) {
                page_t pml1_addr = NULL;
                if(pml2_addr[pml2] == 0) {
                    pml2_addr[pml2] = (uintptr_t)kernel_page_alloc();
                    if(!pml2_addr[pml2]) return false; // Out of memory
                    pml1_addr = (page_t)(pml2_addr[pml2] | KERNEL_MEMORY_MASK); 
                    memset(pml1_addr, 0, PAGE_SIZE);
                    pml2_addr[pml2] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | (flags & KERNEL_PFLAG_USER);
                } else {
                    pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2] | KERNEL_MEMORY_MASK);
                }

                for(; pml1 < KERNEL_PAGE_ENTRIES; pml1++) {
                    // if(pml1_addr[pml1] != 0) {
                    //     // printf("Memory already mapped!");
                    //     return false; // Memory already allocated 
                    // }
                    pml1_addr[pml1] = phys | flags;
                    pages_count--;
                    phys += PAGE_SIZE;
                    if(pages_count==0) return true; // We filled up everything
                }
                pml1 = 0;
            }
            pml2 = 0;
        }
        pml3 = 0;
    }
    return pages_count == 0;
}

// TODO: Fix XD. XD may not be supported always so checks to remove it are necessary
bool page_alloc(page_t pml4_addr, uintptr_t virt, size_t pages_count, pageflags_t flags) {
    virt &= ~PAGE_MASK;
    uint16_t pml1 = (virt >> (12   )) & 0x1ff;
    uint16_t pml2 = (virt >> (12+9 )) & 0x1ff;
    uint16_t pml3 = (virt >> (12+18)) & 0x1ff;
    uint16_t pml4 = (virt >> (12+27)) & 0x1ff;
    for(; pml4 < KERNEL_PAGE_ENTRIES; pml4++) {
        page_t pml3_addr = NULL;
        if(pml4_addr[pml4] == 0) {
            pml4_addr[pml4] = (uintptr_t)kernel_page_alloc();
            if(!pml4_addr[pml4]) return false; // Out of memory
            pml3_addr = (page_t)(pml4_addr[pml4] | KERNEL_MEMORY_MASK); 
            memset(pml3_addr, 0, PAGE_SIZE);
            pml4_addr[pml4] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | (flags & KERNEL_PFLAG_USER);
        } else {
            pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4] | KERNEL_MEMORY_MASK);
        }

        for(; pml3 < KERNEL_PAGE_ENTRIES; pml3++) {
            page_t pml2_addr = NULL;
            if(pml3_addr[pml3] == 0) {
                pml3_addr[pml3] = (uintptr_t)kernel_page_alloc();
                if(!pml3_addr[pml3]) return false; // Out of memory
                pml2_addr = (page_t)(pml3_addr[pml3] | KERNEL_MEMORY_MASK); 
                memset(pml2_addr, 0, PAGE_SIZE);
                pml3_addr[pml3] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | (flags & KERNEL_PFLAG_USER);
            } else {
                pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3] | KERNEL_MEMORY_MASK);
            }

            for(; pml2 < KERNEL_PAGE_ENTRIES; pml2++) {
                page_t pml1_addr = NULL;
                if(pml2_addr[pml2] == 0) {
                    pml2_addr[pml2] = (uintptr_t)kernel_page_alloc();
                    if(!pml2_addr[pml2]) return false; // Out of memory
                    pml1_addr = (page_t)(pml2_addr[pml2] | KERNEL_MEMORY_MASK); 
                    memset(pml1_addr, 0, PAGE_SIZE);
                    pml2_addr[pml2] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | (flags & KERNEL_PFLAG_USER);
                } else {
                    pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2] | KERNEL_MEMORY_MASK);
                }

                for(; pml1 < KERNEL_PAGE_ENTRIES; pml1++) {
#if 0
                    if(pml1_addr[pml1] != 0) {
                        printf("Memory already allocated!\n");
                        return false; // Memory already allocated 
                    }
#endif
                    if(pml1_addr[pml1] == 0) {
                        pml1_addr[pml1] = (uintptr_t)kernel_page_alloc();
                        if(pml1_addr[pml1] == 0) return false;
                        pml1_addr[pml1] |= flags | KERNEL_PFLAG_PRESENT;
                    }
                    pages_count--;
                    if(pages_count==0) return true; // We filled up everything
                }
                pml1 = 0;
            }
            pml2 = 0;
        }
        pml3 = 0;
    }
    return pages_count == 0;
}
// FIXME: Consider cleaning up after yourself in case kernel_page_alloc fails? Not that its very recoverable but ykwim
// Or maybe leave it up to the user who can do page_unalloc and clean it up?
bool page_share(page_t parent, page_t child, uintptr_t virt, size_t pages_count) {
    if(pages_count==0) return true;
    virt &= ~PAGE_MASK;
    uint16_t pml1 = (virt >> (12   )) & 0x1ff;
    uint16_t pml2 = (virt >> (12+9 )) & 0x1ff;
    uint16_t pml3 = (virt >> (12+18)) & 0x1ff;
    uint16_t pml4 = (virt >> (12+27)) & 0x1ff;
    for(; pml4 < KERNEL_PAGE_ENTRIES; ++pml4) {
         if(parent[pml4] == 0) continue;
         page_t pml3_child;
         if(child[pml4] == 0) {
             child[pml4] = (uintptr_t)kernel_page_alloc();
             if(!child[pml4]) return false; // Out of memory
             pml3_child = (page_t)PAGE_ALIGN_DOWN(child[pml4] | KERNEL_MEMORY_MASK);
             memset(pml3_child, 0, PAGE_SIZE);
             child[pml4] |= parent[pml4] & KERNEL_PFLAGS_MASK; 
         } else {
             pml3_child = (page_t)PAGE_ALIGN_DOWN(child[pml4] | KERNEL_MEMORY_MASK);
         }
         page_t pml3_parent = (page_t)PAGE_ALIGN_DOWN(parent[pml4] | KERNEL_MEMORY_MASK);
         for(; pml3 < KERNEL_PAGE_ENTRIES; ++pml3) {
              if(pml3_parent[pml3] == 0) continue;
              page_t pml2_child;
              if(pml3_child[pml3] == 0) {
                  pml3_child[pml3] = (uintptr_t)kernel_page_alloc();
                  if(!pml3_child[pml3]) return false; // Out of memory
                  pml2_child = (page_t)PAGE_ALIGN_DOWN(pml3_child[pml3] | KERNEL_MEMORY_MASK);
                  memset(pml2_child, 0, PAGE_SIZE);
                  pml3_child[pml3] |= pml3_parent[pml3] & KERNEL_PFLAGS_MASK; 
              } else {
                  pml2_child = (page_t)PAGE_ALIGN_DOWN(pml3_child[pml3] | KERNEL_MEMORY_MASK);
              }
              page_t pml2_parent = (page_t)PAGE_ALIGN_DOWN(pml3_parent[pml3] | KERNEL_MEMORY_MASK);
              for(; pml2 < KERNEL_PAGE_ENTRIES; ++pml2) {
                   if(pml2_parent[pml2] == 0) continue;
                   page_t pml1_child;
                   if(pml2_child[pml2] == 0) {
                       pml2_child[pml2] = (uintptr_t)kernel_page_alloc();
                       if(!pml2_child[pml2]) return false; // Out of memory
                       pml1_child = (page_t)PAGE_ALIGN_DOWN(pml2_child[pml2] | KERNEL_MEMORY_MASK);
                       memset(pml1_child, 0, PAGE_SIZE);
                       pml2_child[pml2] |= pml2_parent[pml2] & KERNEL_PFLAGS_MASK; 
                   } else {
                       pml1_child = (page_t)PAGE_ALIGN_DOWN(pml2_child[pml2] | KERNEL_MEMORY_MASK);
                   }
                   page_t pml1_parent = (page_t)PAGE_ALIGN_DOWN(pml2_parent[pml2] | KERNEL_MEMORY_MASK);
                   for(; pml1 < KERNEL_PAGE_ENTRIES; ++pml1) {
                        if(pml1_parent[pml1] == 0) continue;
                        pml1_child[pml1] = pml1_parent[pml1];
                        pages_count--;
                        if(pages_count==0) return true;
                   }
              }
         }
    }
    return pages_count==0;
}
void page_join(page_t parent, page_t child) {
    for(size_t pml4 = 0; pml4 < KERNEL_PAGE_ENTRIES; ++pml4) {
         if(parent[pml4] == 0) continue;
         if(child[pml4] == 0) {
             child[pml4] = parent[pml4];
             continue;
         }
         page_t pml3_parent = (page_t)PAGE_ALIGN_DOWN(parent[pml4] | KERNEL_MEMORY_MASK);
         page_t pml3_child  = (page_t)PAGE_ALIGN_DOWN(child [pml4] | KERNEL_MEMORY_MASK);
         for(size_t pml3 = 0; pml3 < KERNEL_PAGE_ENTRIES; ++pml3) {
              if(pml3_parent[pml3] == 0) continue;
              if(pml3_child[pml3] == 0) {
                  pml3_child[pml3] = pml3_parent[pml3];
                  continue;
              }
              page_t pml2_parent = (page_t)PAGE_ALIGN_DOWN(pml3_parent[pml3] | KERNEL_MEMORY_MASK);
              page_t pml2_child  = (page_t)PAGE_ALIGN_DOWN(pml3_child [pml3] | KERNEL_MEMORY_MASK);
              for(size_t pml2 = 0; pml2 < KERNEL_PAGE_ENTRIES; ++pml2) {
                   if(pml2_parent[pml2] == 0) continue;
                   if(pml2_child[pml2] == 0) {
                       pml2_child[pml2] = pml2_parent[pml2];
                       continue;
                   }
                   page_t pml1_parent = (page_t)PAGE_ALIGN_DOWN(pml2_parent[pml2] | KERNEL_MEMORY_MASK);
                   page_t pml1_child  = (page_t)PAGE_ALIGN_DOWN(pml2_child [pml2] | KERNEL_MEMORY_MASK);
                   for(size_t pml1 = 0; pml1 < KERNEL_PAGE_ENTRIES; ++pml1) {
                        if(pml1_parent[pml1] == 0) continue;
                        if(pml1_child[pml1] != 0) continue;
                        pml1_child[pml1] = pml1_parent[pml1];
                   }
              }
         }
    }
}
void page_destruct(page_t pml4_addr, uint16_t type) {
    kwarn("page_destruct is not good. Will be removed at some point");
}

static bool page_empty(page_t page, uint16_t until) {
    if(until==0) return true;
    for(uint16_t i=0; i < until; ++i) {
        if(page[i] != 0) return false;
    }
    return true;
}
void page_unalloc(page_t pml4_addr, uintptr_t virt, size_t pages_count) {
    if(pages_count==0) return;
    virt &= ~PAGE_MASK;
    uint16_t pml1_start = (virt >> (12   )) & 0x1ff;
    uint16_t pml2_start = (virt >> (12+9 )) & 0x1ff;
    uint16_t pml3_start = (virt >> (12+18)) & 0x1ff;
    uint16_t pml4_start = (virt >> (12+27)) & 0x1ff;
    for(uint16_t pml4=pml4_start; pml4 < KERNEL_PAGE_ENTRIES; pml4++) {
        page_t pml3_addr;
        paddr_t pml3_phys; 
        if(pml4_addr[pml4] == 0) continue;
        pml3_phys = pml4_addr[pml4];
        pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml3_phys | KERNEL_MEMORY_MASK);
        for(uint16_t pml3=pml3_start; pml3 < KERNEL_PAGE_ENTRIES; pml3++) {
            page_t pml2_addr;
            paddr_t pml2_phys; 
            if(pml3_addr[pml3] == 0) continue;
            pml2_phys = pml3_addr[pml3];
            pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml2_phys | KERNEL_MEMORY_MASK);

            for(uint16_t pml2=pml2_start; pml2 < KERNEL_PAGE_ENTRIES; pml2++) {
                page_t pml1_addr;
                paddr_t pml1_phys; 
                if(pml2_addr[pml2] == 0) continue;
                pml1_phys = pml2_addr[pml2];
                pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml1_phys | KERNEL_MEMORY_MASK);
                for(uint16_t pml1=pml1_start; pml1 < KERNEL_PAGE_ENTRIES; pml1++) {
                    paddr_t pml0_phys; 
                    if(pml1_addr[pml1] == 0) continue; 
                    pml0_phys = pml1_addr[pml1];
                    pml1_addr[pml1] = 0;
                    kernel_page_dealloc(pml0_phys);
                    pages_count--;
                    if(pages_count==0) return; // We filled up everything
                }
                if(page_empty(pml1_addr, pml1_start)) {
                    pml2_addr[pml2] = 0;
                    kernel_page_dealloc(pml1_phys);
                }
                pml1_start = 0;
            }
            if(page_empty(pml2_addr, pml2_start)) {
                pml3_addr[pml3] = 0;
                kernel_page_dealloc(pml2_phys);
            }
            pml2_start = 0;
        }
        if(page_empty(pml3_addr, pml3_start)) {
            pml4_addr[pml4] = 0;
            kernel_page_dealloc(pml3_phys);
        }
        pml3_start = 0;
    }
    // We did not unmap everything
}
void page_unmap(page_t pml4_addr, uintptr_t virt, size_t pages_count) {
    if(pages_count==0) return;
    virt &= ~PAGE_MASK;
    uint16_t pml1_start = (virt >> (12   )) & 0x1ff;
    uint16_t pml2_start = (virt >> (12+9 )) & 0x1ff;
    uint16_t pml3_start = (virt >> (12+18)) & 0x1ff;
    uint16_t pml4_start = (virt >> (12+27)) & 0x1ff;
    for(uint16_t pml4=pml4_start; pml4 < KERNEL_PAGE_ENTRIES; pml4++) {
        page_t pml3_addr;
        paddr_t pml3_phys; 
        if(pml4_addr[pml4] == 0) continue;
        pml3_phys = pml4_addr[pml4];
        pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml3_phys | KERNEL_MEMORY_MASK);
        for(uint16_t pml3=pml3_start; pml3 < KERNEL_PAGE_ENTRIES; pml3++) {
            page_t pml2_addr;
            paddr_t pml2_phys; 
            if(pml3_addr[pml3] == 0) continue;
            pml2_phys = pml3_addr[pml3];
            pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml2_phys | KERNEL_MEMORY_MASK);

            for(uint16_t pml2=pml2_start; pml2 < KERNEL_PAGE_ENTRIES; pml2++) {
                page_t pml1_addr;
                paddr_t pml1_phys; 
                if(pml2_addr[pml2] == 0) continue;
                pml1_phys = pml2_addr[pml2];
                pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml1_phys | KERNEL_MEMORY_MASK);
                for(uint16_t pml1=pml1_start; pml1 < KERNEL_PAGE_ENTRIES; pml1++) {
                    if(pml1_addr[pml1] == 0) continue; 
                    pml1_addr[pml1] = 0;
                    pages_count--;
                    if(pages_count==0) return; // We filled up everything
                }
                if(page_empty(pml1_addr, pml1_start)) {
                    pml2_addr[pml2] = 0;
                    kernel_page_dealloc(pml1_phys);
                }
                pml1_start = 0;
            }
            if(page_empty(pml2_addr, pml2_start)) {
                pml3_addr[pml3] = 0;
                kernel_page_dealloc(pml2_phys);
            }
            pml2_start = 0;
        }
        if(page_empty(pml3_addr, pml3_start)) {
            pml4_addr[pml4] = 0;
            kernel_page_dealloc(pml3_phys);
        }
        pml3_start = 0;
    }
    // We did not unmap everything
}
uintptr_t virt_to_phys(page_t pml4_addr, uintptr_t addr) {
    uint16_t pml1 = (addr >> (12   )) & 0x1ff;
    uint16_t pml2 = (addr >> (12+9 )) & 0x1ff;
    uint16_t pml3 = (addr >> (12+18)) & 0x1ff;
    uint16_t pml4 = (addr >> (12+27)) & 0x1ff;
    page_t pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4]);
    if(!pml3_addr) return 0;
    pml3_addr = (page_t)((uintptr_t)pml3_addr | KERNEL_MEMORY_MASK);

    page_t pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3]);
    if(!pml2_addr) return 0;
    pml2_addr = (page_t)((uintptr_t)pml2_addr | KERNEL_MEMORY_MASK);

    page_t pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2]);
    if(!pml1_addr) return 0;

    pml1_addr = (page_t)((uintptr_t)pml1_addr | KERNEL_MEMORY_MASK);

    return PAGE_ALIGN_DOWN(pml1_addr[pml1]);
}

extern uint8_t section_text_begin[];
extern uint8_t section_text_end[];

extern uint8_t section_const_data_begin[];
extern uint8_t section_const_data_end[];

extern uint8_t section_mut_data_begin[];
extern uint8_t section_mut_data_end[];

void init_paging() {
    Mempair addr_resp = {0};
    kernel_mempair(&addr_resp);
    paddr_t pml4_phys = kernel_page_alloc(); 
    if(!pml4_phys) {
       kpanic("ERROR: Could not allocate page map. Ran out of memory");
       kabort();
    }
    kernel.pml4 = (page_t)(pml4_phys | KERNEL_MEMORY_MASK);
    memset(kernel.pml4, 0, PAGE_SIZE);
    boot_map_phys_memory(); // Bootloader specific
    uintptr_t phys, virt;
    size_t len;

    phys = PAGE_ALIGN_DOWN(addr_resp.phys + (((uintptr_t)section_text_begin) - addr_resp.virt));
    virt = PAGE_ALIGN_DOWN((uintptr_t)section_text_begin);
    len  = (PAGE_ALIGN_UP((uintptr_t)section_text_end) - PAGE_ALIGN_DOWN((uintptr_t)section_text_begin))/PAGE_SIZE;
    kinfo("Mapping .text   (%p -> %p) %zu pages", (void*)phys, (void*)virt, len);
    if(!page_mmap(kernel.pml4, phys, virt, len, KERNEL_PFLAG_PRESENT)) {
        kpanic("Failed to map .text");
    }

    phys = PAGE_ALIGN_DOWN(addr_resp.phys + (((uintptr_t)section_const_data_begin) - addr_resp.virt));
    virt = PAGE_ALIGN_DOWN((uintptr_t)section_const_data_begin);
    len  = (PAGE_ALIGN_UP((uintptr_t)section_const_data_end) - PAGE_ALIGN_DOWN((uintptr_t)section_const_data_begin))/PAGE_SIZE;
    kinfo("Mapping .rodata (%p -> %p) %zu pages", (void*)phys, (void*)virt, len);
    if(!page_mmap(kernel.pml4, phys, virt, len, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE)) {
        kpanic("Failed to map .rodata");
    }

    phys = PAGE_ALIGN_DOWN(addr_resp.phys + (((uintptr_t)section_mut_data_begin) - addr_resp.virt));
    virt = PAGE_ALIGN_DOWN((uintptr_t)section_mut_data_begin);
    len = (PAGE_ALIGN_UP((uintptr_t)section_mut_data_end) - PAGE_ALIGN_DOWN((uintptr_t)section_mut_data_begin))/PAGE_SIZE;
    kinfo("Mapping .data   (%p -> %p) %zu pages", (void*)phys, (void*)virt, len);
    if(!page_mmap(kernel.pml4, phys, virt, len, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_EXEC_DISABLE)) {
        kpanic("Failed to map .data");
    }
    kinfo("Allocating stack (%p) %zu pages", (void*)KERNEL_STACK_ADDR, KERNEL_STACK_PAGES);
    if(!page_alloc(kernel.pml4, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE)) {
        kpanic("Failed to map/allocate the stack");
    }
    asm volatile(
    	"movl $0x277, %%ecx;"
    	"rdmsr;"
    	"movw $0x0401, %%dx;"
    	"wrmsr;"
    	::: "eax", "ecx", "edx", "memory"
    );
}



void page_flags_serialise(pageflags_t flags, char* buf, size_t cap) {
    assert(cap >= 7);
    memset(buf, 0  ,cap);
    buf[0] = flags & KERNEL_PFLAG_PRESENT          ? 'p' : '-';
    buf[1] = flags & KERNEL_PFLAG_WRITE            ? 'w' : '-';
    buf[2] = flags & KERNEL_PFLAG_USER             ? 'u' : '-';
    buf[3] = flags & KERNEL_PFLAG_WRITE_THROUGH    ? 'w' : '-';
    buf[4] = flags & KERNEL_PFLAG_CACHE_DISABLE    ? 'c' : '-';
    buf[5] = flags & KERNEL_PFLAG_ACCESSED         ? 'a' : '-';
    buf[6] = flags & KERNEL_PFLAG_EXEC_DISABLE     ? 'x' : '-';
}

const char* page_type_str(pageflags_t flags) {
    switch((flags & KENREL_PTYPE_MASK) >> KERNEL_PTYPE_SHIFT) {
    case (KERNEL_PTYPE_USER>>KERNEL_PTYPE_SHIFT):
        return "User";
    case (KERNEL_PTYPE_KERNEL>>KERNEL_PTYPE_SHIFT):
        return "Kernel";
    }
    return "Unknown";
}
