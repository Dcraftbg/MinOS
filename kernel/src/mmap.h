#pragma once
#include "../../config.h"
#include <stddef.h>
#include <stdint.h>
#include "memory.h"
#include "print.h"
#include "utils.h"
#include "assert.h"
#include <limine.h>
typedef struct {
    uint8_t* addr;
    size_t page_count;
    size_t page_available;
} Memmap;
void* memmap_alloc(Memmap* map, size_t pages_count);
void memmap_dealloc(Memmap* map, void* at, size_t pages_count);
void init_memmap();
