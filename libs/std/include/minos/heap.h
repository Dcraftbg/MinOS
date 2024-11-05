#pragma once
enum {
    HEAP_RESIZABLE=0b1,
};
typedef struct {
    void* address;
    size_t size;
} MinOSHeap;
