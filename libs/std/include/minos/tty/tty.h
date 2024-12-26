#pragma once
#include <minos/sysstd.h>
#include "ttydefs.h"
static intptr_t tty_set_flags(uintptr_t handle, ttyflags_t flags) {
    return ioctl(handle, TTY_IOCTL_SET_FLAGS, (void*)(uintptr_t)flags);
}
static intptr_t tty_get_flags(uintptr_t handle, ttyflags_t* flags) {
    return ioctl(handle, TTY_IOCTL_GET_FLAGS, flags);
}
