#include <minos/syscall.h>
#include <minos/syscodes.h>
#include <minos2errno.h>
#include <errno.h>
#include <assert.h>

#define syscall(sys, ...) \
    intptr_t e = sys(__VA_ARGS__); \
    return e < 0 ? (errno = _minos2errno(e), -1) : e

int close(int fd) {
    syscall(syscall1, SYS_CLOSE, fd);
}
ssize_t write(int fd, const void* buf, size_t size) {
    syscall(syscall3, SYS_WRITE, fd, buf, size);
}
ssize_t read(int fd, void* buf, size_t size) {
    syscall(syscall3, SYS_READ, fd, buf, size);
}
off_t lseek(int fd, off_t offset, int whence) {
    syscall(syscall3, SYS_LSEEK, fd, offset, whence);
}
pid_t fork(void) {
    syscall(syscall0, SYS_FORK);
}
void _exit(int status) {
    syscall1(SYS_EXIT, status);
}
int chdir(const char* path) {
    syscall(syscall1, SYS_CHDIR, path);
}
char* getcwd(char* buf, size_t cap) {
    assert(buf && "TODO: getcwd with buf=NULL");
    intptr_t e = syscall2(SYS_GETCWD, buf, cap);
    if(e < 0) {
        errno = _minos2errno(e);
        return NULL;
    }
    return buf;
}

