#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    void **items;
    size_t len, cap;
} PtrDarray;
bool ptr_darray_reserve(PtrDarray* da, size_t extra);
size_t ptr_darray_pick_empty_slot(PtrDarray* da);
