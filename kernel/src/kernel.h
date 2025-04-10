#pragma once
#include "mem/bitmap.h"
#include "page.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/tss.h"
#include "vfs.h"
#include "pit.h"
#include "interrupt.h"
struct Logger;
typedef struct Cache Cache;
#include <sync/mutex.h>
typedef struct {
    Bitmap map;
    Mutex map_lock;
    page_t pml4;

#ifdef GLOBAL_STORAGE_GDT_IDT
    GDT gdt __attribute__((aligned(4096)));
    IDT idt __attribute__((aligned(4096)));
#else
    GDT* gdt; // Allocated in the bitmap
    IDT* idt;
#endif
    TSS tss;
    Superblock rootBlock;
    size_t current_taskid;
    size_t taskid; // Task ID counter
    size_t current_processid; // Cache of tasks[current_taskid]->processid
    size_t processid;
    struct list tasks;
    struct list processes;
    Cache *cache_cache;
    Cache *inode_cache;
    Cache *task_cache;
    Cache *resource_cache;
    Cache *device_cache;
    Cache *memregion_cache;
    Cache *memlist_cache;
    Cache *process_cache;
    Cache *allocation_cache;
    Cache *heap_cache;
    Cache *charqueue_cache;
    Cache *pci_device_cache;
    Cache *cache64, *cache256;

    size_t task_switch_irq;
    IntController* interrupt_controller;
    struct list cache_list;
    struct Logger* logger;
    PitInfo pit_info;
    bool unwinding;
} Kernel;
extern Kernel kernel;


