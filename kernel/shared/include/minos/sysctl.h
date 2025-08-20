#pragma once
#include <stdint.h>
#define MAX_SYSCTL_NAME 64
enum {
    SYSCTL_KERNEL_NAME,
    SYSCTL_MEMINFO,
    SYSCTL_COUNT
};
typedef struct {
    uint64_t total, free;
} SysctlMeminfo;
intptr_t sysctl(uint32_t op, void* arg);
