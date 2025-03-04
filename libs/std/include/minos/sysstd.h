#pragma once
#include "syscall.h"
#include "syscodes.h"
#include "fsdefs.h"
#include "heap.h"
#include "time.h"
#include "epoll.h"

static intptr_t open(const char* path, fmode_t mode, oflags_t flags) {
    return syscall3(SYS_OPEN, path, mode, flags);
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
static intptr_t seek(uintptr_t handle, off_t offset, seekfrom_t from) {
    return syscall3(SYS_SEEK, handle, offset, from);
}
static intptr_t tell(uintptr_t handle) {
    return syscall1(SYS_TELL, handle);
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

static intptr_t heap_create(uint64_t flags, size_t size_min) {
    return syscall2(SYS_HEAP_CREATE, flags, size_min);
}
static intptr_t heap_get(uintptr_t id, MinOSHeap* heap) {
    return syscall2(SYS_HEAP_GET, id, heap);
}
static intptr_t heap_extend(uintptr_t id, size_t bytes) {
    return syscall2(SYS_HEAP_EXTEND, id, bytes);
}
static intptr_t chdir(const char* path) {
    return syscall1(SYS_CHDIR, path);
}

static intptr_t getcwd(char* buf, size_t cap) {
    return syscall2(SYS_GETCWD, buf, cap);
}
static intptr_t stat(const char* path, Stats* stats) {
    return syscall2(SYS_STAT, path, stats);
}
static intptr_t get_dir_entries(uintptr_t dirfd, DirEntry* entries, size_t size) {
    return read(dirfd, entries, size);
}
static intptr_t sleepfor(const MinOS_Duration* duration) {
    return syscall1(SYS_SLEEPFOR, duration);
} 
static intptr_t gettime(MinOS_Time* time) {
    return syscall1(SYS_GETTIME, time);
}
static intptr_t truncate(uintptr_t handle, size_t size) {
    return syscall2(SYS_TRUNCATE, handle, size);
}
static intptr_t _epoll_create1(int flags) {
    return syscall1(SYS_EPOLL_CREATE1, flags);
}
static intptr_t _epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    return syscall4(SYS_EPOLL_CTL, epfd, op, fd, event);
}
static intptr_t _epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return syscall4(SYS_EPOLL_WAIT, epfd, events, maxevents, timeout);
}
