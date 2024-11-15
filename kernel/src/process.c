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
    list_init(&kernel.processes);
}
Process* kernel_process_add() {
    Process* process = (Process*)cache_alloc(kernel.process_cache);
    if (process) {
         memset(process, 0, sizeof(*process));
         process->main_threadid = INVALID_TASK_ID;
         process->id = kernel.processid++;
         list_init(&process->heap_list);
         list_init(&process->list);
         list_append(&process->list, &kernel.processes);
    }
    return process;
} 
void process_drop(Process* process) {
    if(process->resources) resourceblock_dealloc(process->resources);
    if(process->curdir) kernel_dealloc(process->curdir, PATH_MAX);
    Heap* heap = (Heap*)process->heap_list.next;
    while(&heap->list != &process->heap_list) {
        Heap* next_heap = (Heap*)heap->list.next;
        heap_destroy(heap);
        heap = next_heap;
    }
    cache_dealloc(kernel.process_cache, process);
}


Process* get_process_by_id(size_t id) {
    debug_assert(id != INVALID_TASK_ID);
    Process* proc = (Process*)kernel.processes.next;
    while(proc != (Process*)&kernel.processes) {
        if(proc->id == id) return proc;
        proc = (Process*)proc->list.next;
    }
    return NULL;
}

Heap* get_heap_by_id(Process* process, size_t heapid) {
    for(struct list* head = process->heap_list.next; head != &process->heap_list; head = head->next) {
        Heap* heap = (Heap*)head;
        if(heap->id == heapid) return heap;
    }
    return NULL;
}

// FIXME: Possible issues with multiple threads
intptr_t process_heap_extend(Process* process, Heap* heap, size_t extra) {
    intptr_t e=0;
    Task* task = get_task_by_id(process->main_threadid); 
    // Invalidly resized.
    // Checking overflow:
    if(heap->address + (heap->pages+extra)*PAGE_SIZE <= heap->address) return -NOT_ENOUGH_MEM;
    MemoryList* reglist = memlist_find(&task->image.memlist, (void*)heap->address);
    if(!reglist)
        return -INVALID_PARAM;
    
    MemoryRegion* region = reglist->region;
    if(reglist->list.next != &task->image.memlist) {
        MemoryRegion* next = ((MemoryList*)reglist->list.next)->region;
        if(next->address < heap->address + (heap->pages+extra)*PAGE_SIZE) return -NOT_ENOUGH_MEM;
    }
    uintptr_t end = heap->address + (heap->pages)*PAGE_SIZE;
    if(!page_alloc(task->image.cr3, end, extra, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_USER | KERNEL_PFLAG_PRESENT))
        return -NOT_ENOUGH_MEM;
    if((e=heap_extend(heap, extra)) < 0)
        goto err;
    region->pages += extra;
    return 0;
err:
    page_unalloc(task->image.cr3, end, extra);
    return e;
}
