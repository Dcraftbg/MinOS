#pragma once
#include <minos/sysstd.h>
typedef struct {
    uint32_t width, height;
    uint32_t bpp;
} FbStats;
enum {
    FB_IOCTL_GET_STATS=1,
};
#if 0
static intptr_t fbget_stats(intptr_t file, FbStats* stats) {
    return ioctl(file, FB_IOCTL_GET_STATS, stats);
}
#endif
