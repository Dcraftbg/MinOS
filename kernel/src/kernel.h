#pragma once
#include "mem/bitmap.h"
#include "page.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/tss.h"
#include "vfs.h"
#include "pit.h"
struct Logger;
struct Cache;
typedef struct {
    Bitmap map;
    page_t pml4;
    GDT* gdt; // Allocated in the bitmap
    IDT* idt;
    TSS tss;
    Superblock rootBlock;
    size_t current_taskid;
    size_t taskid; // Task ID counter
    size_t current_processid; // Cache of tasks[current_taskid]->processid
    size_t processid;
    struct list tasks;
    struct list processes;
    struct Cache* cache_cache;
    struct Cache* inode_cache;
    struct Cache* task_cache;
    struct Cache* resource_cache;
    struct Cache* device_cache;
    struct Cache* memregion_cache;
    struct Cache* memlist_cache;
    struct Cache* process_cache;
    struct list cache_list;
    struct Logger* logger;
    PitInfo pit_info;
} Kernel;
extern Kernel kernel;


