#pragma once
#include <stdint.h>
#include <stddef.h>
enum {
    MODE_READ  =0b1,
    MODE_WRITE =0b10,
    MODE_STREAM=0b100,
    /*append?*/
};
typedef uint32_t fmode_t;
#define PATH_MAX 4096
enum {
    O_CREAT    =0b1,
    O_DIRECTORY=0b10,
    O_TRUNC    =0b100,
};
typedef uint32_t oflags_t;
enum {
    SEEK_START,
    SEEK_CURSOR,
    SEEK_EOF,
};
typedef uintptr_t seekfrom_t;
typedef intptr_t off_t;
typedef size_t inodeid_t;
typedef enum {
    INODE_DIR,
    INODE_FILE,
    INODE_DEVICE,
    INODE_MINOS_SOCKET,
    INODE_EPOLL,
    INODE_COUNT,
} InodeKind;
typedef int inodekind_t;
typedef struct {
    inodeid_t inodeid;
    inodekind_t kind;
    size_t lba ; // lba is in 1<<lba bytes
    size_t size; // In lba
} Stats;

typedef struct {
    size_t size;
    inodeid_t inodeid;
    inodekind_t kind;
    char name[];
} DirEntry;
typedef uint32_t fflags_t;
enum {
    FFLAGS_NONBLOCK,
};
