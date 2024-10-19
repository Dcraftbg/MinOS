#include "exec.h"
#include "elf.h"
#include "memory.h"
#include "page.h"
#include "vfs.h"
#include "string.h"
#include "fileutils.h"
#include "kernel.h"
#include "debug.h"

#define return_defer_err(x) do {\
    e=(x);\
    goto DEFER_ERR;\
} while(0)


intptr_t fork(Task* task, Task* result, void* frame) {
    assert(task->image.flags & TASK_FLAG_PRESENT);
    intptr_t e=0;
    struct list* list = &task->image.memlist;
    struct list* first = list;
    list = list->next;
    paddr_t cr3_phys;
    if(!(cr3_phys = kernel_page_alloc()))
        return_defer_err(-NOT_ENOUGH_MEM);
    result->image.cr3 = (page_t)(cr3_phys | KERNEL_MEMORY_MASK);
    memset(result->image.cr3, 0, PAGE_SIZE);
    result->image.flags = 0;
    while(first != list) {
        MemoryList* memlist = (MemoryList*)list;
        MemoryRegion* region = memlist->region;
        MemoryRegion* nreg = memregion_clone(region, task->image.cr3, result->image.cr3);
        if(!nreg) 
            return_defer_err(-NOT_ENOUGH_MEM);
        MemoryList* nlist = memlist_new(region);
        if(!nlist) {
            memregion_drop(region, result->image.cr3);
            return_defer_err(-NOT_ENOUGH_MEM);
        }
        memlist_add(&result->image.memlist, nlist);
        list = list->next;
    }
    page_join(kernel.pml4, result->image.cr3);

    result->image.argc  = task->image.argc;
    result->image.argv  = task->image.argv;
    result->image.ts_rsp = frame;
    result->image.rip    = task->image.rip;
    result->image.flags  = task->image.flags & (~(TASK_FLAG_RUNNING));
    return 0;
DEFER_ERR:
    // TODO: Remove this:
    // Destruct cr3, which we should NOT do
    if(result->image.cr3) page_destruct(result->image.cr3, KERNEL_PTYPE_USER);
    if(result) drop_task(result);
    return e;
}
intptr_t exec_new(const char* path, Args* args, Args* env) {    
    intptr_t e=0;
    Process* process = kernel_process_add();
    Task* task = NULL;
    if(!process) return -LIMITS; // Reached max tasks and/or we're out of memory
    process->resources = new_resource_block();
    if(!process->resources) return_defer_err(-NOT_ENOUGH_MEM);
    task = kernel_task_add();
    if(!task) return_defer_err(-LIMITS); // Reached max tasks and/or we're out of memory
    process->main_threadid = task->id;
    task->processid = process->id;
    if((e=exec(task, path, args, env)) < 0)
        return_defer_err(e);
    task->image.flags |= TASK_FLAG_PRESENT;
    return 0;
DEFER_ERR:
    if(task) drop_task(task);
    if(process) process_drop(process);
    return e;
}
static intptr_t args_push(Task* task, Args* args, char** stack_head, char*** argv) {
    // TODO: Kind of interesting but what if you swapped the cr3 AFTER YOU JOINED WITH THE KERNEL MEMORY MAP
    // And then used memcpy, which would be wayyy faster since it doesn't need to manually do page checks;
    //
    // TODO: Squish this into a single operation by calculating where argv would be positioned at using bytelen
    char* args_head = *stack_head;
    intptr_t e=0;
    for(size_t i = 0; i < args->argc; ++i) {
        if(args->argv[i]) {
            size_t len = strlen(args->argv[i]);
            *stack_head -= len+1; // the stack grows backwards
            if((e = user_memcpy(task, *stack_head, args->argv[i], len+1)) < 0) 
                return e;
        }
    }
    *stack_head -= sizeof(args_head) * args->argc; // Reserve space for argv
    *argv = (char**)*stack_head;
    for(size_t i = 0; i < args->argc; ++i) {
        if(args->argv[i]) {
            size_t len = strlen(args->argv[i]);
            args_head -= len+1;
        }
        if((e = user_memcpy(task, (*argv)+i, &args_head, sizeof(args_head))) < 0)
            return e;
    }
    return 0;
}
// TODO: Fix XD. XD may not be supported always so checks to remove it are necessary
intptr_t exec(Task* task, const char* path, Args* args, Args* envs) {
    intptr_t e=0;
    VfsFile file={0};
    bool fopened=false;
    MemoryList* kstack_region = NULL;
    MemoryList* ustack_region = NULL;
    Elf64ProgHeader* pheaders = NULL;
    Elf64Header header;

    if(!path) return -INVALID_PARAM;

    // TODO: Maybe remove this?
    // I don't really see a purpose in calling page_destruct anymore now that we have the 
    // memory regions
    paddr_t cr3_phys; 
    if(!(cr3_phys = kernel_page_alloc())) {
        return_defer_err(-NOT_ENOUGH_MEM);
    }
    task->image.cr3 = (page_t)(cr3_phys | KERNEL_MEMORY_MASK);
    memset(task->image.cr3, 0, PAGE_SIZE);

    task->image.ts_rsp = 0;
    task->image.rip = 0;
    task->image.flags = TASK_FLAG_FIRST_RUN;

    if((e=vfs_open(path, &file, MODE_READ)) < 0) {
        return_defer_err(e);
    }
    fopened = true;

    if((e=read_exact(&file, &header, sizeof(header))) < 0) return_defer_err(e);
    
    if(!elf_header_verify(&header)) return_defer_err(-INVALID_MAGIC);
    
    if(
        header.ident[ELF_DATA_ENCODING] != ELF_DATA_LITTLE_ENDIAN ||
        header.ident[ELF_DATA_CLASS]    != ELF_CLASS_64BIT
    ) return_defer_err(-UNSUPPORTED);

    if(
        header.type != ELF_TYPE_EXEC
    ) return_defer_err(-INVALID_TYPE);


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
                             pheader->p_type, pheader->image.flags,pheader->offset, (void*)pheader->virt_addr, (void*)pheader->phys_addr, pheader->filesize, pheader->memsize, pheader->align);
#endif        
        if (pheader->p_type != ELF_PHREADER_LOAD || pheader->memsize == 0) continue;
        pageflags_t flags = KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER | KERNEL_PTYPE_USER;
        uint64_t regionflags = 0;
        if (!(pheader->flags & ELF_PROG_EXEC)) {
            
            flags |= KERNEL_PFLAG_EXEC_DISABLE;
        }
        if (pheader->flags & ELF_PROG_WRITE) {
            flags |= KERNEL_PFLAG_WRITE;
            regionflags |= MEMREG_WRITE;
        }
        if(pheader->filesize > pheader->memsize) {
            return_defer_err(-SIZE_MISMATCH);
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
        MemoryList* region;
        if(!(region=memlist_new(memregion_new(regionflags, flags, virt, segment_pages)))) {
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return_defer_err(-NOT_ENOUGH_MEM);
        }
        // TODO: Better return message. it could be either Incomplete Map or Not Enough Memory
        // We just don't know right now and Not Enough Memory seems more reasonable
        if (
          !page_mmap(task->image.cr3, virt_to_phys(kernel.pml4, (uintptr_t)memory), virt, segment_pages, flags)
        ) {
           memlist_dealloc(region, NULL);
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return_defer_err(-NOT_ENOUGH_MEM);
        }
        memlist_add(&task->image.memlist, region);
    }
    if (header.entry == 0)
        return_defer_err(-NO_ENTRYPOINT);

    size_t stack_pages = USER_STACK_PAGES + 1 + (PAGE_ALIGN_UP(args->bytelen+env->bytelen) / PAGE_SIZE);

    if(!(ustack_region=memlist_new(memregion_new(MEMREG_WRITE, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER, USER_STACK_ADDR, stack_pages)))) 
        return_defer_err(-NOT_ENOUGH_MEM);
    

    if (!page_alloc(task->image.cr3, USER_STACK_ADDR, stack_pages, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER))  
        return_defer_err(-NOT_ENOUGH_MEM);
    memlist_add(&task->image.memlist, ustack_region);
    
    if(!(kstack_region=memlist_new(memregion_new(MEMREG_WRITE, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES))))
        return_defer_err(-NOT_ENOUGH_MEM);
    
    memlist_add(&task->image.memlist, kstack_region);
    // NOTE: If you're wondering why KERNEL_PTYPE_USER is applied here. The USER program OWNS that memory
    if (!page_alloc(task->image.cr3, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER))
        return_defer_err(-NOT_ENOUGH_MEM);

    char* stack_head = (char*)USER_STACK_PTR;
    char** envp;
    if((e=args_push(task, envs, &stack_head, &envp)) < 0) 
        return_defer_err(e);
    char** argv;
    if((e=args_push(task, args, &stack_head, &argv)) < 0) 
        return_defer_err(e);
    task->image.envc = envs->argc;
    task->image.envv = (const char**)envp;
    task->image.argc = args->argc;
    task->image.argv = (const char**)argv;
    IRQFrame* frame = (IRQFrame*)((virt_to_phys(task->image.cr3, KERNEL_STACK_PTR) | KERNEL_MEMORY_MASK) + sizeof(IRQFrame));
    frame->rip    = header.entry;
    frame->cs     = GDT_USERCODE | 0x3; 
    frame->rflags = 0x202;
    frame->ss     = GDT_USERDATA | 0x3;
    frame->rsp    = (uint64_t)stack_head;
    task->image.ts_rsp = (void*)(KERNEL_STACK_PTR+sizeof(IRQFrame));
    task->image.rip    = header.entry;
    page_join(kernel.pml4, task->image.cr3);
    vfs_close(&file);
    fopened = false;
    return 0;
DEFER_ERR:
    if(pheaders) kernel_dealloc(pheaders, prog_header_size);
    if(kstack_region) memlist_dealloc(kstack_region, NULL);
    if(ustack_region) memlist_dealloc(ustack_region, NULL);
    if(fopened) vfs_close(&file);
    if(task->image.cr3) page_destruct(task->image.cr3, KERNEL_PTYPE_USER);
    return e;
}
