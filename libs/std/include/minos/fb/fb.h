#pragma once
#include <minos/sysstd.h>
typedef struct {
    uint32_t width, height;
    uint32_t bpp;
} FbStats;
enum {
    FB_IOCTL_GET_STATS=1,
};
static intptr_t fbget_stats(uintptr_t handle, FbStats* stats) {
    return ioctl(handle, FB_IOCTL_GET_STATS, stats);
}
