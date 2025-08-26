#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t width, height;
    uint32_t bpp;
    uintptr_t pitch_bytes;
} FbStats;
enum {
    FB_IOCTL_GET_STATS=1,
};
#define fbget_stats(fd, stats) ioctl(fd, FB_IOCTL_GET_STATS, stats)
