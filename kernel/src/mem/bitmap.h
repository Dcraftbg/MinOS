#pragma once
#include "../../config.h"
#include <stddef.h>
#include <stdint.h>
#include "memory.h"
#include "print.h"
#include "utils.h"
#include "assert.h"
typedef struct {
    uint8_t* addr;
    size_t page_count;
    size_t page_available;
} Bitmap;
void* bitmap_alloc(Bitmap* map, size_t pages_count);
void bitmap_dealloc(Bitmap* map, void* at, size_t pages_count);
void init_bitmap();
