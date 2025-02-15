#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <minos/sysstd.h>
struct DIR {
    struct dirent dirent;
    int fd;
    char buf[2048];
    size_t left;
};
DIR* opendir(const char* path) {
    DIR *dir = malloc(sizeof(DIR));
    if(!dir) {
        errno = ENOMEM;
        goto malloc_err;
    }
    memset(dir, 0, sizeof(DIR));
    intptr_t e;
    if((e = open(path, MODE_READ, O_DIRECTORY)) < 0) {
        errno = _status_to_errno(e);
        goto open_err;
    }
    dir->fd = e;
    return dir;
open_err:
    free(dir);
malloc_err:
    return NULL;
}
struct dirent* readdir(DIR *dir) {
    if(dir->left == 0) {
        intptr_t e = get_dir_entries(dir->fd, (DirEntry*)dir->buf, sizeof(dir->buf));
        if(e < 0) {
            errno = _status_to_errno(e);
            return NULL;
        }
        // We reached EOF
        if(e == 0) return NULL;
        assert(e <= sizeof(dir->buf));
        dir->left = e;
        return readdir(dir);
    }
    DirEntry* entry = (DirEntry*)dir->buf;
    assert(entry->size <= dir->left);
    if(entry->size < sizeof(DirEntry)) {
        errno = EINVAL;
        return NULL;
    }
    // Entry with a name thats way too large
    if(entry->size - sizeof(DirEntry) > 256) {
        errno = ENOBUFS;
        return NULL;
    }
    dir->dirent.d_ino = entry->inodeid;
    // TODO: d_type field using entry->kind
    dir->dirent.d_type = DT_UNKNOWN;
    memcpy(dir->dirent.d_name, entry->name, entry->size - sizeof(DirEntry));
    dir->dirent.d_name[entry->size-sizeof(DirEntry)] = '\0';
    dir->left -= entry->size;
    memmove(dir->buf, dir->buf + entry->size, dir->left);
    return &dir->dirent;
}
// TODO: actual error. Not that it matters but yeah
int closedir(DIR *dir) {
    if(!dir) return -1;
    close(dir->fd);
    free(dir);
    return 0;
}
