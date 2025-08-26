#pragma once
#include <collections/list.h>
#include "task.h"
#include "page.h"
#include "resource.h"
#include "ptr_darray.h"
#include <sync/mutex.h>
#include <stddef.h>

typedef struct {
    size_t bytelen; // Calculated using argv[0..argc].sum(arg.len());
    size_t argc;
    const char** argv;
} Args;
Args create_args(const char** argv);
#define PROC_FLAG_DYING 0b1
#define INVALID_PROCESS_ID -1
#define MAX_CHILD_PROCESSES 16
// TODO: Introduce pid_t (taskid_t) and other process/task types under
// proctypes.h
typedef struct {
    uint32_t exit_code;
    uint32_t id;
} ChildProcess;
#define CHILD_EXITCODE_MASK 0b01111111111111111111111111111111
#define CHILD_DEAD_MASK     0b10000000000000000000000000000000
#define child_process_is_dead(cp)             ((cp).exit_code & CHILD_DEAD_MASK)
#define child_process_mark_dead(cp)           ((cp).exit_code |= CHILD_DEAD_MASK)
#define child_process_get_exit_code(cp)       ((cp).exit_code & CHILD_EXITCODE_MASK)
#define child_process_set_exit_code(cp, code) ((cp).exit_code = ((cp).exit_code & (~CHILD_EXITCODE_MASK)) | ((code) & CHILD_EXITCODE_MASK))
#define child_process_get_id(cp)              ((cp).id)
#define child_process_set_id(cp, _id)         ((cp).id = (_id))

typedef struct Process Process;
struct Process {
    struct list list;
    Process* parent;
    size_t id; 
    Task* main_thread;
    uint64_t flags;
    ResourceBlock* resources;
    Inode* curdir_inode;
    char* curdir /*[PATH_MAX]*/;
    ChildProcess children[MAX_CHILD_PROCESSES];
    PtrDarray shared_memory;
    Mutex shared_memory_mutex;
};

void init_processes();
Process* kernel_process_add(); 
void process_drop(Process* process);
typedef struct MemoryList MemoryList;
intptr_t process_memreg_extend(Process* process, MemoryList* reglist, size_t extra);

static Process* current_process() {
    return current_task()->process;
}
