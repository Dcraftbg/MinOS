#pragma once
#include <stdint.h>
#include <stddef.h>

// Convenient wrapper
#define WM_METHODS(T) \
    extern size_t min_##T##_size,\
                  max_##T##_size;\
    size_t size_##T (const T* payload);\
    void cleanup_##T (T* payload); \
    ssize_t read_memory_##T (void* buf, size_t payload_size, T* payload); \
    ssize_t write_##T (void* fd, ssize_t (*write)(void* fd, const void* data, size_t n), const T* payload);

typedef struct WmCreateWindowInfo {
    uint32_t x, y;
    uint32_t width, height;
    uint32_t min_width, min_height;
    uint32_t max_width, max_height;
    uint32_t flags;
    uint32_t title_len;
    char* title;
} WmCreateWindowInfo;
WM_METHODS(WmCreateWindowInfo);

typedef struct WmCreateSHMRegion {
    uint64_t size;
} WmCreateSHMRegion;
WM_METHODS(WmCreateSHMRegion);

typedef struct WmDrawSHMRegion {
    uint32_t window; 
    uint32_t window_x, window_y;
    uint32_t shm_key;
    uint32_t width, height;
    // MUST be aligned by 4
    uint64_t pitch_bytes;
    // MUST be aligned by 4
    uint64_t offset_bytes;
} WmDrawSHMRegion;
WM_METHODS(WmDrawSHMRegion);

// SC (Server-Client) packets
typedef struct WmEvent {
    uint32_t window;
    uint16_t event;
    uint16_t _reserved0;
    uint64_t arg0;
    uint64_t arg1;
} WmEvent;
WM_METHODS(WmEvent);
