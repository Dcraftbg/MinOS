#include "process.h"
#include "assert.h"
#include "kernel.h"
#include "mem/slab.h"
#include "string.h"
#include "port.h"
#include "pic.h"
#include "exec.h"
#include "log.h"

void init_processes() {
    assert(kernel.process_cache = create_new_cache(sizeof(Process), "Process"));
}
Process* kernel_process_add() {
    mutex_lock(&kernel.processes_mutex);
    if(!ptr_darray_reserve(&kernel.processes, 1)) {
        mutex_unlock(&kernel.processes_mutex);
        return NULL;
    }
    size_t id = ptr_darray_pick_empty_slot(&kernel.processes);
    Process* process = (Process*)cache_alloc(kernel.process_cache);
    if (!process) {
        mutex_unlock(&kernel.processes_mutex);
        return NULL;
    }
    if(id == kernel.processes.len) kernel.processes.len++;
    memset(process, 0, sizeof(*process));
    for(size_t i = 0; i < ARRAY_LEN(process->children); ++i) {
        child_process_set_id(process->children[i], INVALID_PROCESS_ID);
    }
    process->parent = NULL;
    process->main_thread = NULL;
    process->id = id;
    list_init(&process->list);
    kernel.processes.items[id] = process;
    mutex_unlock(&kernel.processes_mutex);
    return process;
} 
void process_drop(Process* process) {
    if(process->resources) resourceblock_dealloc(process->resources);
    if(process->curdir) kernel_dealloc(process->curdir, PATH_MAX);
    if(process->curdir_inode) idrop(process->curdir_inode);
    cache_dealloc(kernel.process_cache, process);
}

// FIXME: Possible issues with multiple threads
intptr_t process_memreg_extend(Process* process, MemoryList* reglist, size_t extra) {
    Task* task = process->main_thread; 
    MemoryRegion* region = reglist->region;
    // TODO: bring this back
    // Invalidly resized.
    // Checking overflow:
    // if(heap->address + (heap->pages+extra)*PAGE_SIZE <= heap->address) return -NOT_ENOUGH_MEM;
    if(reglist->list.next != &task->memlist) {
        MemoryRegion* next = ((MemoryList*)reglist->list.next)->region;
        if(next->address < region->address + (region->pages+extra)*PAGE_SIZE) return -NOT_ENOUGH_MEM;
    }
    uintptr_t end = region->address + (region->pages)*PAGE_SIZE;
    if(!page_alloc(task->cr3, end, extra, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_USER | KERNEL_PFLAG_PRESENT))
        return -NOT_ENOUGH_MEM;
    region->pages += extra;
    return 0;
}
Args create_args(const char** argv) {
    size_t sum = 0;
    size_t argc = 0;
    while(argv[argc]) {
        sum += strlen(argv[argc++]) + 1;
    }
    return (Args) { sum, argc, argv };
}
