#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <errno.h>
#include <minos2errno.h>
#include <minos/sysstd.h>

int fstat(int fd, struct stat* statbuf) {
    struct statx statx = { 0 };
    intptr_t e = _fstatx(fd, STATX_INO | STATX_SIZE | STATX_TYPE, &statx);
    if(e < 0) return (errno = _minos2errno(e), -1);
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_ino = statx.stx_ino;
    statbuf->st_size = statx.stx_size;
    if(statx.stx_mask & STATX_TYPE) {
        switch(statx.stx_type) {
        case STX_TYPE_DIR:
            statbuf->st_mode = S_IFDIR;
            break;
        case STX_TYPE_FILE:
            statbuf->st_mode = S_IFREG;
            break;
        case STX_TYPE_DEVICE:
            statbuf->st_mode = S_IFCHR;
            break;
        // Some may not have a unix equivilent...
        case STX_TYPE_EPOLL:
        }
    }
    return 0;
}
int stat(const char* pathname, struct stat* statbuf) {
    int fd = open(pathname, O_RDONLY);
    if(fd < 0) return fd;
    int e = fstat(fd, statbuf);
    close(fd);
    return e;
}
// Only for external linkage
#undef lstat
int lstat(const char* pathname, struct stat* statbuf) {
    return stat(pathname, statbuf);
}
