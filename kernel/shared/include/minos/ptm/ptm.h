#pragma once
enum {
    PTM_IOCTL_MKPTTY = 1,
};
#define ptm_mkptty(fd) ioctl(fd, PTM_IOCTL_MKPTTY, (void*)0)
