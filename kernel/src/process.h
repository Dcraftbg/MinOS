#pragma once
#include "list.h"
#include "task.h"
#include "page.h"
#include "resource.h"
#include <stddef.h>
#include "heap.h"

typedef struct {
    size_t bytelen; // Calculated using argv[0..argc].sum(arg.len());
    size_t argc;
    const char** argv;
} Args;
static Args create_args(size_t argc, const char** argv) {
    size_t sum = 0;
    for(size_t i = 0; i < argc; ++i) {
        sum += strlen(argv[i])+1;
    }
    return (Args) { sum, argc, argv };
}

#define PROC_FLAG_DYING 0b1
#define INVALID_PROCESS_ID -1
typedef struct {
    struct list list;
    size_t id; 
    size_t main_threadid;
    ResourceBlock* resources;
    uint64_t flags;
    int64_t exit_code;
    size_t heapid; // Current heapid count
    struct list heap_list;
    inodeid_t curdir_id;
    Superblock* curdir_sb;
    char* curdir /*[PATH_MAX]*/;
} Process;

void init_processes();
Process* kernel_process_add(); 
void process_drop(Process* process);
Process* get_process_by_id(size_t id);
Heap* get_heap_by_id(Process* process, size_t heapid);

static Process* current_process() {
    return get_process_by_id(kernel.current_processid);
}
