#include "exec.h"
#include "elf.h"
#include "memory.h"
#include "page.h"
#include "vfs.h"
#include "string.h"
#include "fileutils.h"
#include "kernel.h"
#include "log.h"
// Picking a processor to run on
#include "load_balance.h"
#include "apic.h"

#define return_defer_err(x) do {\
    e=(x);\
    goto DEFER_ERR;\
} while(0)


intptr_t fork(Task* task, Task* result, void* frame) {
    assert(task->flags & TASK_FLAG_PRESENT);
    intptr_t e=0;
    struct list* list = &task->memlist;
    struct list* first = list;
    list = list->next;
    paddr_t cr3_phys;
    if(!(cr3_phys = kernel_page_alloc()))
        return_defer_err(-NOT_ENOUGH_MEM);
    result->cr3 = (page_t)(cr3_phys | KERNEL_MEMORY_MASK);
    memset(result->cr3, 0, PAGE_SIZE);
    result->flags = 0;
    while(first != list) {
        MemoryList* memlist = (MemoryList*)list;
        MemoryRegion* region = memlist->region;
        MemoryRegion* nreg = memregion_clone(region, task->cr3, result->cr3);
        if(!nreg) 
            return_defer_err(-NOT_ENOUGH_MEM);
        MemoryList* nlist = memlist_new(region);
        if(!nlist) {
            memregion_drop(region, result->cr3);
            return_defer_err(-NOT_ENOUGH_MEM);
        }
        memlist_add(&result->memlist, nlist);
        list = list->next;
    }
    page_join(kernel.pml4, result->cr3);

    result->argc  = task->argc;
    result->argv  = task->argv;
    result->ts_rsp = frame;
    result->rip    = task->rip;
    result->flags  = task->flags & (~(TASK_FLAG_RUNNING));
    size_t processor_id = pick_processor_for_task();
    Processor* processor = &kernel.processors[processor_id];
    if(processor_id == get_lapic_id()) {
        disable_interrupts();
        list_insert(&result->list, &processor->scheduler.queue);
        enable_interrupts();
    } else {
        mutex_lock(&processor->scheduler.queue_mutex);
        list_insert(&result->list, &processor->scheduler.queue);
        mutex_unlock(&processor->scheduler.queue_mutex);
    }
    return 0;
DEFER_ERR:
    // TODO: Remove this:
    // Destruct cr3, which we should NOT do
    if(result->cr3) page_destruct(result->cr3, KERNEL_PTYPE_USER);
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
    if((e=fetch_inode(&kernel.rootBlock, kernel.rootBlock.root, &process->curdir_inode)) < 0) return_defer_err(e); 
    process->main_thread = task;
    
    task->process = process;
    process->curdir = kernel_malloc(PATH_MAX);
    if(!process->curdir)
        return_defer_err(-NOT_ENOUGH_MEM);
    process->curdir[0] = '/';
    process->curdir[1] = '\0';
    Path p;
    if((e=parse_abs(path, &p)) < 0)
        return_defer_err(e);
    if((e=exec(task, &p, args, env)) < 0)
        return_defer_err(e);
    task->flags |= TASK_FLAG_PRESENT;
    return process->id;
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
    *stack_head -= sizeof(args_head) * (args->argc + 1); // Reserve space for argv + 1
    *argv = (char**)*stack_head;
    for(size_t i = 0; i < args->argc; ++i) {
        if(args->argv[i]) {
            size_t len = strlen(args->argv[i]);
            args_head -= len+1;
        }
        if((e = user_memcpy(task, (*argv)+i, &args_head, sizeof(args_head))) < 0)
            return e;
    }
    // As per libc compliancy. Add NULL to the end 
    char* null = NULL;
    if((e = user_memcpy(task, (*argv)+args->argc, &null, sizeof(null))) < 0)
         return e;
    return 0;
}
static intptr_t load_header(Elf64Header* header, Inode* file) {
    intptr_t e;
    if((e=read_exact(file, header, sizeof(*header), 0)) < 0) return e;
    if(!elf_header_verify(header)) return -INVALID_MAGIC;
    if(
        header->ident[ELF_DATA_ENCODING] != ELF_DATA_LITTLE_ENDIAN ||
        header->ident[ELF_DATA_CLASS]    != ELF_CLASS_64BIT
    ) return -UNSUPPORTED;
    if(
        header->type != ELF_TYPE_EXEC && header->type != ELF_TYPE_DYNAMIC
    ) {
        return -INVALID_TYPE;
    }
    return 0;
}
static intptr_t load_pheaders(Elf64ProgHeader** pheaders_result, const Elf64Header* header, Inode* file) {
    intptr_t e;
    size_t prog_header_size = header->phnum * sizeof(Elf64ProgHeader);
    Elf64ProgHeader* pheaders = kernel_malloc(prog_header_size);
    if(!pheaders) return -NOT_ENOUGH_MEM;
    memset(pheaders, 0, prog_header_size);    
    if(
      (e=read_exact(file, pheaders, prog_header_size, header->phoff)) < 0
    ) {
        kernel_dealloc(pheaders, prog_header_size);
        return e;
    }
    *pheaders_result = pheaders;
    return 0;
}
static intptr_t load_elf(Task* task, Inode* file, uintptr_t offset, Elf64Header* header, Elf64ProgHeader** pheaders_result, bool is_interp) {
    intptr_t e = 0;
    if((e=load_header(header, file)) < 0) return e;
    if((e=load_pheaders(pheaders_result, header, file)) < 0) return e;
    Elf64ProgHeader* pheaders = *pheaders_result;
    uintptr_t eoe = 0;
    bool interp = false;
    char interp_buffer[1024] = { 0 };
    for(size_t i = 0; i < header->phnum; ++i) {
        Elf64ProgHeader* pheader = &pheaders[i];
        switch(pheader->p_type) {
        case ELF_PHEADER_LOAD: {
            uintptr_t end = PAGE_ALIGN_UP(pheader->virt_addr + pheader->memsize);
            if(end > eoe) eoe = end;
        } break;
        case ELF_PHEADER_INTERP: {
            if(pheader->filesize > sizeof(interp_buffer)-1) {
                kerror("Interpreter header overflows buffer");
                return -BUFFER_TOO_SMALL;
            }
            if((e=read_exact(file, interp_buffer, pheader->filesize, pheader->offset)) < 0) return e;
            interp = true;
        } break;
        }
    }
    if(is_interp && interp) {
        kerror("Interpreter has its own interpreter? We don't allow that :(");
        return -RECURSIVE_ELF_INTERP;
    }
    if(interp) {
        Inode* interp_file = NULL;
        Path path;
        if((e=parse_abs(interp_buffer, &path)) < 0) return e;
        if((e=vfs_find(&path, &interp_file)) < 0) return e;
        kernel_dealloc(pheaders, header->phnum * sizeof(Elf64ProgHeader));
        *pheaders_result = pheaders = NULL;
        // kinfo("Relocating interpreter `%s` at %p", interp_buffer, eoe);
        e = load_elf(task, interp_file, eoe, header, pheaders_result, true);
        idrop(interp_file);
        return e;
    }
    task->eoe = eoe + offset;
    for(size_t i = 0; i < header->phnum; ++i) {
        Elf64ProgHeader* pheader = &pheaders[i];
#if 0
        printf("Elf64ProgHeader { p_type: 0x%08X, flags: 0x%08X, offset: %12zu, virt_addr: %p, phys_addr: %p, filesize: %6zu, memsize: %4zu, align: %4zu }\n",
                             pheader->p_type, pheader->flags,pheader->offset, (void*)pheader->virt_addr, (void*)pheader->phys_addr, pheader->filesize, pheader->memsize, pheader->align);
#endif        
        if (pheader->p_type != ELF_PHEADER_LOAD || pheader->memsize == 0) continue;
        pageflags_t flags = KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER | KERNEL_PTYPE_USER;
        if (!(pheader->flags & ELF_PROG_EXEC)) {
            flags |= KERNEL_PFLAG_EXEC_DISABLE;
        }
        if (pheader->flags & ELF_PROG_WRITE) {
            flags |= KERNEL_PFLAG_WRITE;
        }
        if(pheader->filesize > pheader->memsize) {
            return -SIZE_MISMATCH;
        }
        pheader->virt_addr += offset;
        uintptr_t segment_pages  = (PAGE_ALIGN_UP(pheader->virt_addr + pheader->memsize) - PAGE_ALIGN_DOWN(pheader->virt_addr)) / PAGE_SIZE;
        uintptr_t virt = PAGE_ALIGN_DOWN(pheader->virt_addr);
        uintptr_t virt_off = pheader->virt_addr - virt;
        void* memory = kernel_malloc(segment_pages * PAGE_SIZE);
        if(!memory)
           return -NOT_ENOUGH_MEM;
        
        memset(memory, 0, segment_pages * PAGE_SIZE);
        if (
          (e = read_exact(file, memory+virt_off, pheader->filesize, pheader->offset)) < 0
        ) {
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return e;
        }
        MemoryList* region;
        if(!(region=memlist_new(memregion_new(flags, virt, segment_pages)))) {
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return -NOT_ENOUGH_MEM;
        }
        // TODO: Better return message. it could be either Incomplete Map or Not Enough Memory
        // We just don't know right now and Not Enough Memory seems more reasonable
        if (
          !page_mmap(task->cr3, virt_to_phys(kernel.pml4, (uintptr_t)memory), virt, segment_pages, flags)
        ) {
           memlist_dealloc(region, NULL);
           kernel_dealloc(memory, segment_pages * PAGE_SIZE);
           return -NOT_ENOUGH_MEM;
        }
        memlist_add(&task->memlist, region);
    }
    if (header->entry == 0)
        return -NO_ENTRYPOINT;
    header->entry += offset;
    return 0;
}
// TODO: Fix XD. XD may not be supported always so checks to remove it are necessary
intptr_t exec(Task* task, Path* path, Args* args, Args* envs) {
    intptr_t e=0;
    Inode* file=NULL;
    Elf64Header header;
    Elf64ProgHeader* pheaders = NULL;
    MemoryList* kstack_region = NULL;
    MemoryList* ustack_region = NULL;

    if(!path) return -INVALID_PARAM;

    // TODO: Maybe remove this?
    // I don't really see a purpose in calling page_destruct anymore now that we have the 
    // memory regions
    paddr_t cr3_phys; 
    if(!(cr3_phys = kernel_page_alloc())) {
        return_defer_err(-NOT_ENOUGH_MEM);
    }
    task->cr3 = (page_t)(cr3_phys | KERNEL_MEMORY_MASK);
    memset(task->cr3, 0, PAGE_SIZE);

    task->ts_rsp = 0;
    task->rip = 0;
    task->flags = TASK_FLAG_FIRST_RUN;

    if((e=vfs_find(path, &file)) < 0) return_defer_err(e);
    if((e=load_elf(task, file, 0, &header, &pheaders, false)) < 0) return_defer_err(e);
    
    size_t stack_pages = USER_STACK_PAGES + 1 + (PAGE_ALIGN_UP(args->bytelen+envs->bytelen) / PAGE_SIZE);

    if(!(ustack_region=memlist_new(memregion_new(KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER, USER_STACK_ADDR, stack_pages)))) 
        return_defer_err(-NOT_ENOUGH_MEM);
    

    if (!page_alloc(task->cr3, USER_STACK_ADDR, stack_pages, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_USER | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER))  
        return_defer_err(-NOT_ENOUGH_MEM);
    memlist_add(&task->memlist, ustack_region);
    
    if(!(kstack_region=memlist_new(memregion_new(KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES))))
        return_defer_err(-NOT_ENOUGH_MEM);
    
    memlist_add(&task->memlist, kstack_region);
    // NOTE: If you're wondering why KERNEL_PTYPE_USER is applied here. The USER program OWNS that memory
    if (!page_alloc(task->cr3, KERNEL_STACK_ADDR, KERNEL_STACK_PAGES, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PTYPE_USER))
        return_defer_err(-NOT_ENOUGH_MEM);

    char* stack_head = (char*)USER_STACK_PTR;
    char** envp;
    if((e=args_push(task, envs, &stack_head, &envp)) < 0) 
        return_defer_err(e);
    stack_head = (char*)(((((uintptr_t)stack_head))/16)*16);
    char** argv;
    if((e=args_push(task, args, &stack_head, &argv)) < 0) 
        return_defer_err(e);
    stack_head = (char*)(((((uintptr_t)stack_head))/16)*16);
    task->envc = envs->argc;
    task->envv = (const char**)envp;
    task->argc = args->argc;
    task->argv = (const char**)argv;
    IRQFrame* frame = (IRQFrame*)((virt_to_phys(task->cr3, KERNEL_STACK_PTR) | KERNEL_MEMORY_MASK) + sizeof(IRQFrame));
    frame->rip    = header.entry;
    frame->cs     = GDT_USERCODE | 0x3; 
    frame->rflags = 0x202;
    frame->ss     = GDT_USERDATA | 0x3;
    frame->rsp    = (uint64_t)stack_head;
    task->ts_rsp = (void*)(KERNEL_STACK_PTR+sizeof(IRQFrame));
    task->rip    = header.entry;
    disable_interrupts();
    page_join(kernel.pml4, task->cr3);
    enable_interrupts();
    idrop(file);
    file = NULL;
    kernel_dealloc(pheaders, header.phnum * sizeof(Elf64ProgHeader));
    size_t processor_id = pick_processor_for_task();
    Processor* processor = &kernel.processors[processor_id];
    if(processor_id == get_lapic_id()) {
        disable_interrupts();
        list_insert(&task->list, &processor->scheduler.queue);
        enable_interrupts();
    } else {
        mutex_lock(&processor->scheduler.queue_mutex);
        list_insert(&task->list, &processor->scheduler.queue);
        mutex_unlock(&processor->scheduler.queue_mutex);
    }
    return 0;
DEFER_ERR:
    if(pheaders) kernel_dealloc(pheaders, header.phnum * sizeof(Elf64ProgHeader));
    if(kstack_region) memlist_dealloc(kstack_region, NULL);
    if(ustack_region) memlist_dealloc(ustack_region, NULL);
    if(file) idrop(file);
    if(task->cr3) page_destruct(task->cr3, KERNEL_PTYPE_USER);
    return e;
}
