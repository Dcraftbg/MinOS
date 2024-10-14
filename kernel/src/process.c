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
