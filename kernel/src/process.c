#include "process.h"
#include "assert.h"
#include "kernel.h"
#include "slab.h"
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
         list_init(&process->list);
         list_append(&process->list, &kernel.processes);
    }
    return process;
} 
void process_drop(Process* process) {
    if(process->resources) resourceblock_dealloc(process->resources);
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
