#pragma once
#include "list.h"
#include "task.h"
#include "page.h"
#include "resource.h"
#include <stddef.h>

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


#define INVALID_PROCESS_ID -1
typedef struct {
    struct list list;
    size_t id; 
    size_t main_threadid;
} Process;

void init_processes();
Process* kernel_process_add(); 
void process_drop(Process* process);
