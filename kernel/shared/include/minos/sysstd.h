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
