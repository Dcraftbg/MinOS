#pragma once
enum {
    HEAP_RESIZABLE=0b1,
    HEAP_EXACT=0b10,
};
typedef struct {
    void* address;
    size_t size;
} MinOSHeap;
