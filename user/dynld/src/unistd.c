#include <unistd.h>
#include <minos/syscall.h>
#include <minos/syscodes.h>
int open(const char* path, int flags, ...) {
    // Unused mode
    return syscall2(SYS_OPEN, path, flags);
}
int close(int fd) {
    return syscall1(SYS_CLOSE, fd);
}
ssize_t write(int fd, const void* buf, size_t size) {
    return syscall3(SYS_WRITE, fd, buf, size);
}
ssize_t read(int fd, void* buf, size_t size) {
    return syscall3(SYS_READ, fd, buf, size);
}
off_t lseek(int fd, off_t offset, int whence) {
    return syscall3(SYS_LSEEK, fd, offset, whence);
}
void _exit(int status) {
    syscall1(SYS_EXIT, status);
}
intptr_t _mmap(void** addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return syscall6(SYS_MMAP, addr, length, prot, flags, fd, offset);
}
