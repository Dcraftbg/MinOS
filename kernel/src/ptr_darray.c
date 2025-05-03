#include "ptr_darray.h"
#include "string.h"
#include "memory.h"
#include "log.h"

bool ptr_darray_reserve(PtrDarray* da, size_t extra) {
    if(da->len + extra > da->cap) {
        size_t new_cap = PAGE_ALIGN_UP((da->cap*2 + extra) * sizeof(da->items[0])) / sizeof(da->items[0]);
        void* new_items = kernel_malloc(new_cap * sizeof(da->items[0]));
        if(!new_items) return false;
        memcpy(new_items, da->items, sizeof(da->items[0]) * da->len);
        kernel_dealloc(da->items, da->cap * sizeof(da->items[0]));
        da->cap = new_cap;
        da->items = new_items;
    }
    return true;
}

size_t ptr_darray_pick_empty_slot(PtrDarray* da) {
    for(size_t i = 0; i < da->len; ++i) {
        if(!da->items[i]) return i;
    }
    return da->len; 
}
