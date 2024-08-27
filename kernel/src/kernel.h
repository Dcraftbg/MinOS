#pragma once
#include "mmap.h"
#include "page.h"
#include "gdt.h"
#include "idt.h"
#include "tss.h"
#include "vfs.h"
struct Cache;
typedef struct {
    Memmap map;
    page_t pml4;
    GDT* gdt; // Allocated in the bitmap
    IDT* idt;
    TSS tss;
    Superblock rootBlock;
    size_t current_taskid;
    size_t taskid; // Task ID counter
    struct list tasks;
    struct Cache* cache_cache;
    struct Cache* inode_cache;
    struct Cache* task_cache;
    struct Cache* resource_cache;
    struct Cache* device_cache;
    struct list cache_list;
} Kernel;
extern Kernel kernel;

