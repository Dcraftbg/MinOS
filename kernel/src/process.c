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
    rwlock_begin_write(&kernel.processes_rwlock);
    if(!ptr_darray_reserve(&kernel.processes, 1)) {
        rwlock_end_write(&kernel.processes_rwlock);
        return NULL;
    }
    size_t id = ptr_darray_pick_empty_slot(&kernel.processes);
    Process* process = (Process*)cache_alloc(kernel.process_cache);
    if (!process) {
        rwlock_end_write(&kernel.processes_rwlock);
        return NULL;
    }
    if(id == kernel.processes.len) kernel.processes.len++;
    memset(process, 0, sizeof(*process));
    for(size_t i = 0; i < ARRAY_LEN(process->children); ++i) {
        child_process_set_id(process->children[i], INVALID_PROCESS_ID);
    }
    process->parentid = INVALID_PROCESS_ID;
    process->main_threadid = INVALID_TASK_ID;
    process->id = id;
    list_init(&process->heap_list);
    list_init(&process->list);
    kernel.processes.items[id] = process;
    rwlock_end_write(&kernel.processes_rwlock);
    return process;
} 
void process_drop(Process* process) {
    if(process->resources) resourceblock_dealloc(process->resources);
    if(process->curdir) kernel_dealloc(process->curdir, PATH_MAX);
    if(process->curdir_inode) idrop(process->curdir_inode);
    Heap* heap = (Heap*)process->heap_list.next;
    while(&heap->list != &process->heap_list) {
        Heap* next_heap = (Heap*)heap->list.next;
        heap_destroy(heap);
        heap = next_heap;
    }
    cache_dealloc(kernel.process_cache, process);
}


Process* get_process_by_id(size_t id) {
    if(id == INVALID_TASK_ID) return NULL;
    rwlock_begin_read(&kernel.processes_rwlock);
    if(id >= kernel.processes.len) {
        rwlock_end_read(&kernel.processes_rwlock);
        return NULL;
    }
    Process* proc = kernel.processes.items[id];
    rwlock_end_read(&kernel.processes_rwlock);
    return proc;
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
