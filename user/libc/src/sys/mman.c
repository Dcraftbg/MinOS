#include <sys/mman.h>
#include <minos/sysstd.h>
#include <minos2errno.h>
#include <errno.h>
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    void* result = addr;
    intptr_t e = _mmap(&result, length, prot, flags, fd, offset);
    if(e < 0) {
        errno = _minos2errno(e);
        return MAP_FAILED;
    }
    return result;
}
