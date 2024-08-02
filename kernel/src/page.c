#include "page.h"
#include "memory.h"
#include "kernel.h"
#include "elf.h"
#include "string.h"
#define PAGE_MASK 0xFFFF000000000000

bool page_mmap(page_t pml4_addr, uintptr_t phys, uintptr_t virt, size_t pages_count, uint16_t flags) {
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
            pml4_addr[pml4] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | flags;
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
                pml3_addr[pml3] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | flags;
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
                    pml2_addr[pml2] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | flags;
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
bool page_alloc(page_t pml4_addr, uintptr_t virt, size_t pages_count, uint16_t flags) {
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
            pml4_addr[pml4] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | flags;
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
                pml3_addr[pml3] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | flags;
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
                    pml2_addr[pml2] |= KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | flags;
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
    for(size_t pml4 = 0; pml4 < KERNEL_PAGE_ENTRIES; ++pml4) {
         if(pml4_addr[pml4] == 0) continue;
         page_t pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4] | KERNEL_MEMORY_MASK);

         for(size_t pml3 = 0; pml3 < KERNEL_PAGE_ENTRIES; ++pml3) {
              if(pml3_addr[pml3] == 0) continue;
              page_t pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3] | KERNEL_MEMORY_MASK);

              for(size_t pml2 = 0; pml2 < KERNEL_PAGE_ENTRIES; ++pml2) {
                   if(pml2_addr[pml2] == 0) continue;
                   page_t pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2] | KERNEL_MEMORY_MASK);

                   for(size_t pml1 = 0; pml1 < KERNEL_PAGE_ENTRIES; ++pml1) {
                        if(pml1_addr[pml1] == 0) continue;
                        if((pml1_addr[pml1] & KENREL_PTYPE_MASK) == type) {
                            kernel_page_dealloc((page_t)PAGE_ALIGN_DOWN(pml1_addr[pml1]));
                            pml1_addr[pml1] = 0;
                        }
                   }

                   if((pml2_addr[pml2] & KENREL_PTYPE_MASK) == type) {
                       kernel_page_dealloc((page_t)PAGE_ALIGN_DOWN((uintptr_t)pml2_addr[pml2]));
                       pml2_addr[pml2] = 0;
                   }
              }

              if((pml3_addr[pml3] & KENREL_PTYPE_MASK) == type) {
                  kernel_page_dealloc((page_t)PAGE_ALIGN_DOWN((uintptr_t)pml3_addr[pml3]));
                  pml3_addr[pml3] = 0;
              }
         }

         if((pml4_addr[pml4] & KENREL_PTYPE_MASK) == type) {
             kernel_page_dealloc((page_t)PAGE_ALIGN_DOWN((uintptr_t)pml4_addr[pml4]));
             pml4_addr[pml4] = 0;
         }
    }
}
uintptr_t virt_to_phys(page_t pml4_addr, uintptr_t addr) {
    uint16_t pml1 = (addr >> (12   )) & 0x1ff;
    uint16_t pml2 = (addr >> (12+9 )) & 0x1ff;
    uint16_t pml3 = (addr >> (12+18)) & 0x1ff;
    uint16_t pml4 = (addr >> (12+27)) & 0x1ff;
    page_t pml3_addr = (page_t)PAGE_ALIGN_DOWN(pml4_addr[pml4] | KERNEL_MEMORY_MASK);
    if(!pml3_addr) return 0;

    page_t pml2_addr = (page_t)PAGE_ALIGN_DOWN(pml3_addr[pml3] | KERNEL_MEMORY_MASK);
    if(!pml2_addr) return 0;

    page_t pml1_addr = (page_t)PAGE_ALIGN_DOWN(pml2_addr[pml2] | KERNEL_MEMORY_MASK);
    if(!pml1_addr) return 0;

    return PAGE_ALIGN_DOWN(pml1_addr[pml1]);
}
void init_paging() {
    Mempair addr_resp = {0};
    kernel_mempair(&addr_resp);
    kernel.pml4 = kernel_page_alloc();
    if(!kernel.pml4) {
       printf("ERROR: Could not allocate page map. Ran out of memory");
       kabort();
    }
    kernel.pml4 = (page_t)(((uintptr_t)kernel.pml4) | KERNEL_MEMORY_MASK);
    memset(kernel.pml4, 0, PAGE_SIZE);
    boot_map_phys_memory(); // Bootloader specific
    void* file_data = NULL;
    size_t file_size = 0;
    kernel_file_data(&file_data, &file_size); 
    assert(file_size > sizeof(Elf64Header));
    Elf64Header* header = (Elf64Header*)file_data;
    assert(elf_header_verify(header)); 
    assert(header->ident[ELF_DATA_ENCODING] == ELF_DATA_LITTLE_ENDIAN);
    assert(header->ident[ELF_DATA_CLASS] == ELF_CLASS_64BIT);
    size_t prog_header_size = header->phnum * sizeof(Elf64ProgHeader);
    assert(header->phoff+prog_header_size < file_size);
    Elf64ProgHeader* headers = (Elf64ProgHeader*)((uint8_t*)file_data+header->phoff);
    for(size_t i = 0; i < header->phnum; ++i) {
        Elf64ProgHeader* h = &headers[i]; // I know it can be done with headers+i. I want to keep things simple for if others read it
        if(h->p_type == 0 || h->memsize == 0) continue;
        uint16_t flags = KERNEL_PFLAG_PRESENT;
        if(h->flags & ELF_PROG_WRITE) {
            flags |= KERNEL_PFLAG_WRITE;
        }
        uintptr_t segment_offset = PAGE_ALIGN_DOWN(h->virt_addr) - addr_resp.virt;
        uintptr_t segment_pages  = (PAGE_ALIGN_UP(h->virt_addr+h->memsize) - PAGE_ALIGN_DOWN(h->virt_addr)) / PAGE_SIZE;
        uintptr_t phys = PAGE_ALIGN_DOWN(addr_resp.phys + segment_offset);
        uintptr_t virt = PAGE_ALIGN_DOWN(addr_resp.virt + segment_offset);
        assert(page_mmap(kernel.pml4, phys, virt, segment_pages, flags));
#if 0
        printf("%zu> Mapping Program Header:\n",i);
        printf(" (Elf)   virtual address  => %p\n",h->virt_addr);
        printf(" (Elf)   physical address => %p\n",h->phys_addr);
        printf(" (Real)  virtual address  => %p\n",virt);
        printf(" (Real)  physical address => %p\n",phys);
#endif
    } 
    assert(page_alloc(kernel.pml4, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE));
}
void update_post_paging() {
    kernel.map.addr = (uint8_t*)(((uintptr_t)kernel.map.addr) | KERNEL_MEMORY_MASK);
}
