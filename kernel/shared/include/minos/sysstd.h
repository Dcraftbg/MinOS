#pragma once
#include "syscall.h"
#include "syscodes.h"
#include "fsdefs.h"
#include "heap.h"
#include "time.h"
#include "epoll.h"
#include "socket.h"
#include "stat.h"

static intptr_t wait_pid(size_t pid) {
    return syscall1(SYS_WAITPID, pid);
}
static intptr_t sleepfor(const MinOS_Duration* duration) {
    return syscall1(SYS_SLEEPFOR, duration);
} 
static intptr_t gettime(MinOS_Time* time) {
    return syscall1(SYS_GETTIME, time);
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
