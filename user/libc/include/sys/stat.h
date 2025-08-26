#pragma once
#include <minos/stat.h>
// Most of these fields are 0 but are here for future expansion
struct stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    uid_t     st_uid; 
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;
};
int stat(const char* pathname, struct stat* statbuf);
int fstat(int fd, struct stat* statbuf);
int lstat(const char* pathname, struct stat* statbuf);
// lstat still exists in the libc but its only for external linkage
#define lstat stat
int fstatx(int fd, unsigned int mask, struct statx* stats);
// For unix compatibility we have some of these even tho they might not be
// even reachable
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
