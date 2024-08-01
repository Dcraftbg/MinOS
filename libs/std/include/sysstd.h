#pragma once
#include "syscall.h"
#include "syscodes.h"

enum {
    MODE_READ=1,
    MODE_WRITE=2,
    /*append?*/
};
typedef uint32_t fmode_t;

static intptr_t open(const char* path, fmode_t mode) {
    return syscall2(SYS_OPEN, path, mode);
}

static intptr_t write(uintptr_t handle, const void* buf, size_t size) {
    return syscall3(SYS_WRITE, handle, buf, size);
}

static intptr_t read(uintptr_t handle, void* buf, size_t size) {
    return syscall3(SYS_READ, handle, buf, size);
}
static intptr_t close(uintptr_t handle) {
    return syscall1(SYS_CLOSE, handle);
}
