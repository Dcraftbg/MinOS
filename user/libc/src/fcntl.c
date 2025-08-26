#include <minos/syscall.h>
#include <minos/syscodes.h>
#include <minos2errno.h>
#include <errno.h>

#define syscall(sys, ...) \
    intptr_t e = sys(__VA_ARGS__); \
    return e < 0 ? (errno = _minos2errno(e), -1) : e
int open(const char* path, int flags, ...) {
    // Unused mode
    syscall(syscall2, SYS_OPEN, path, flags);
}

