#pragma once
#include "syscall.h"
#include "syscodes.h"
#include "fsdefs.h"
#include "heap.h"
#include "time.h"
#include "epoll.h"
#include "socket.h"

static intptr_t open(const char* path, oflags_t flags) {
    return syscall2(SYS_OPEN, path, flags);
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
static intptr_t _mmap(void** addr, size_t length, uint32_t prot, uint32_t flags, uintptr_t fd, off_t offset) {
    return syscall6(SYS_MMAP, addr, length, prot, flags, fd, offset);
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
    return syscall3(SYS_GET_DIR_ENTRIES, dirfd, entries, size);
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
static intptr_t _socket(uint32_t domain, uint32_t type, uint32_t prototype) {
    return syscall3(SYS_SOCKET, domain, type, prototype);
}
static intptr_t _send(uintptr_t sockfd, const void *buf, size_t len) {
    return syscall3(SYS_SEND, sockfd, buf, len);
}
static intptr_t _recv(uintptr_t sockfd,       void *buf, size_t len) {
    return syscall3(SYS_RECV, sockfd, buf, len);
}
static intptr_t _accept(uintptr_t sockfd, struct sockaddr* addr, size_t *addrlen) {
    return syscall3(SYS_ACCEPT, sockfd, addr, addrlen);
}
static intptr_t _bind(uintptr_t sockfd, struct sockaddr* addr, size_t addrlen) {
    return syscall3(SYS_BIND, sockfd, addr, addrlen);
}
static intptr_t _listen(uintptr_t sockfd, size_t n) {
    return syscall2(SYS_LISTEN, sockfd, n);
}
static intptr_t _connect(uintptr_t sockfd, const struct sockaddr* addr, size_t addrlen) {
    return syscall3(SYS_CONNECT, sockfd, addr, addrlen);
}
static intptr_t _shmcreate(size_t size) {
    return syscall1(SYS_SHMCREATE, size);
}
static intptr_t _shmmap(size_t key, void** addr) {
    return syscall2(SYS_SHMMAP, key, addr);
}
static intptr_t _shmrem(size_t key) {
    return syscall1(SYS_SHMREM, key);
}
static intptr_t _sysctl(uint32_t op, void* arg) {
    return syscall2(SYS_SYSCTL, op, arg);
}
