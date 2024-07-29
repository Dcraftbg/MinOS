#include "exec.h"
#include "elf.h"
#include "memory.h"
#include "page.h"
#include "gdt.h"
#include "vfs.h"
#include "string.h"
#include "kernel.h"

#define return_defer_err(x) do {\
    e=(x);\
    goto DEFER_ERR;\
} while(0)

static intptr_t read_exact(VfsFile* file, void* bytes, size_t amount) {
    while(amount) {
        size_t rb = vfs_read(file, bytes, amount);
        if(rb < 0) return rb;
        if(rb == 0) return -PREMATURE_EOF;
        amount-=rb;
        bytes+=rb;
    }
    return 0;
}
// TODO: Fix inconcistencies in the types for rip and rsp; Choose between uintptr_t or void*
// TODO: "User" type in the AVL bits in the page table
// So memory management is ezpz
// TODO: FIXME: cleanup cr3 in case of an error
intptr_t exec(const char* path) {
    intptr_t e=0;
    VfsFile file={0};
    bool fopened=false;
    Task* task = {0};
    Elf64ProgHeader* pheaders = NULL;
    if(!path) return -INVALID_PARAM;
    task = kernel_task_add();
    if(!task) return -LIMITS; // Reached max tasks and/or we're out of memory
    task->cr3 = NULL;
    task->ts_rsp = NULL;
    task->rip = NULL;
    task->flags |= TASK_FLAG_FIRST_RUN;

    if((e=vfs_open(path, &file, MODE_READ)) < 0) {
        return_defer_err(e);
    }
    fopened = true;

    Elf64Header header;
    if((e=read_exact(&file, &header, sizeof(header))) < 0) return_defer_err(e);
    
    if(!elf_header_verify(&header)) return_defer_err(-INVALID_MAGIC);
    
    if(
        header.ident[ELF_DATA_ENCODING] != ELF_DATA_LITTLE_ENDIAN ||
        header.ident[ELF_DATA_CLASS]    != ELF_CLASS_64BIT
    ) return_defer_err(-UNSUPPORTED);

    if(
        header.type != ELF_TYPE_EXEC
    ) return_defer_err(-INVALID_TYPE);
     
    if(!(task->cr3 = kernel_page_alloc())) {
      return_defer_err(-NOT_ENOUGH_MEM);
    }

    task->cr3 = (page_t)((uint64_t)task->cr3 | KERNEL_MEMORY_MASK);
    memset(task->cr3, 0, PAGE_SIZE);

    size_t prog_header_size = header.phnum * sizeof(Elf64ProgHeader);
    pheaders = kernel_malloc(prog_header_size);
    if(!pheaders) {
      return_defer_err(-NOT_ENOUGH_MEM);
    }
    memset(pheaders, 0, prog_header_size);    

    if(
      (e=vfs_seek(&file, header.phoff, SEEK_START)) < 0 ||
      (e=read_exact(&file, pheaders, prog_header_size)) < 0
    )
      return_defer_err(e);
    
    for(size_t i = 0; i < header.phnum; ++i) {
        Elf64ProgHeader* pheader = &pheaders[i];
#if 0
        printf("Elf64ProgHeader { p_type: 0x%08X, flags: 0x%08X, offset: %12zu, virt_addr: %p, phys_addr: %p, filesize: %6zu, memsize: %4zu, align: %4zu }\n",
                             pheader->p_type, pheader->flags,pheader->offset, (void*)pheader->virt_addr, (void*)pheader->phys_addr, pheader->filesize, pheader->memsize, pheader->align);
#endif        
        if (pheader->p_type != ELF_PHREADER_LOAD || pheader->memsize == 0) continue;
        uint16_t flags = KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER;
        if (pheader->flags & ELF_PROG_WRITE) {
            flags |= KERNEL_PFLAG_WRITE;
        }
        uintptr_t segment_pages  = (PAGE_ALIGN_UP(pheader->virt_addr + pheader->memsize) - PAGE_ALIGN_DOWN(pheader->virt_addr)) / PAGE_SIZE;
        uintptr_t virt = PAGE_ALIGN_DOWN(pheader->virt_addr);
        uintptr_t virt_off = pheader->virt_addr - virt;

        void* memory = kernel_malloc(segment_pages * PAGE_SIZE);
        if(!memory)
           return_defer_err(-NOT_ENOUGH_MEM);
        
        memset(memory, 0, segment_pages * PAGE_SIZE);
        if (
          (e = vfs_seek(&file, pheader->offset, SEEK_START)) < 0 ||
          (e = read_exact(&file, memory+virt_off, pheader->filesize)) < 0
        ) {
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return_defer_err(e);
        }
        // TODO: Better return message. it could be either Incomplete Map or Not Enough Memory
        // We just don't know right now and Not Enough Memory seems more reasonable
        if (
          !page_mmap(task->cr3, virt_to_phys(kernel.pml4, (uintptr_t)memory), virt, segment_pages, flags)
        ) {
           printf("Dis mapping %p (%zu pages)?\n",(void*)virt,segment_pages);
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return_defer_err(-NOT_ENOUGH_MEM);
        }
    }
    if (header.entry == 0)
        return_defer_err(-NO_ENTRYPOINT);

    if (!page_alloc(task->cr3, USER_STACK_ADDR  , USER_STACK_PAGES+1, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER))  
        return_defer_err(-NOT_ENOUGH_MEM);

    if (!page_alloc(task->cr3, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT)) 
        return_defer_err(-NOT_ENOUGH_MEM);
    
    IRQFrame* frame = (IRQFrame*)((virt_to_phys(task->cr3, KERNEL_STACK_PTR) | KERNEL_MEMORY_MASK) + sizeof(IRQFrame));
    frame->rip    = header.entry;
    frame->cs     = GDT_USERCODE | 0x3; 
    frame->rflags = 0x202;
    frame->ss     = GDT_USERDATA | 0x3;
    frame->rsp    = USER_STACK_PTR;
    task->ts_rsp = (void*)(KERNEL_STACK_PTR+sizeof(IRQFrame));
    task->rip    = (void*)header.entry;
    task->flags |= TASK_FLAG_PRESENT;
    if(!(task->resources = new_resource_block()))
        return_defer_err(-NOT_ENOUGH_MEM);
    page_join(kernel.pml4, task->cr3);
    vfs_close(&file);
    return 0;
DEFER_ERR:
    if(pheaders) kernel_dealloc(pheaders, prog_header_size);
    // if(task->cr3) page_destruct(task->cr3)
    if(fopened) vfs_close(&file);
    if(task->resources) delete_resource_block(task->resources);
    if(task) drop_task(task);
    return e;
}
