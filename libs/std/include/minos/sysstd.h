#pragma once
#include "syscall.h"
#include "syscodes.h"
#include "fsdefs.h"
#include "heap.h"

static intptr_t open(const char* path, fmode_t mode) {
    return syscall2(SYS_OPEN, path, mode);
}

static intptr_t write(uintptr_t handle, const void* buf, size_t size) {
    return syscall3(SYS_WRITE, handle, buf, size);
}
static intptr_t read(uintptr_t handle, void* buf, size_t size) {
    return syscall3(SYS_READ, handle, buf, size);
}
static intptr_t ioctl(uintptr_t handle, uint32_t op, void* arg) {
    return syscall3(SYS_IOCTL, handle, op, arg);
}
static intptr_t mmap(uintptr_t handle, void** addr, size_t size) {
    return syscall3(SYS_MMAP, handle, addr, size);
}

static intptr_t close(uintptr_t handle) {
    return syscall1(SYS_CLOSE, handle);
}

static intptr_t fork(void) {
    return syscall0(SYS_FORK);
}

static intptr_t execve(const char* path, const char** argv, size_t argc, const char** envp, size_t envc) {
    return syscall5(SYS_EXEC, path, argv, argc, envp, envc);
}

static void _exit(int64_t code) {
    syscall1(SYS_EXIT, code);
}

static intptr_t wait_pid(size_t pid) {
    return syscall1(SYS_WAITPID, pid);
}

static intptr_t heap_create(uint64_t flags) {
    return syscall1(SYS_HEAP_CREATE, flags);
}
static intptr_t heap_allocate(size_t id, size_t size, void** result) {
    return syscall3(SYS_HEAP_ALLOCATE, id, size, result);
}
static intptr_t heap_deallocate(size_t id, void* addr) {
    return syscall2(SYS_HEAP_DEALLOCATE, id, addr);
}

static intptr_t chdir(const char* path) {
    return syscall1(SYS_CHDIR, path);
}

static intptr_t getcwd(char* buf, size_t cap) {
    return syscall2(SYS_GETCWD, buf, cap);
}
