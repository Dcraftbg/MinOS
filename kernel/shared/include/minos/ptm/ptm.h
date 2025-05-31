#pragma once
enum {
    PTM_IOCTL_MKPTTY = 1,
};
#include <stdint.h>
#include <minos/sysstd.h>
static intptr_t ptm_mkptty(uintptr_t handle) {
    return ioctl(handle, PTM_IOCTL_MKPTTY, (void*)0);
}
