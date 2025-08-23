#pragma once
#include <stdint.h>
#include <stddef.h>
#include "stat.h"
#define PATH_MAX 4096
enum {
    O_CREAT     = 0b1,
    O_DIRECTORY = 0b10,
    O_TRUNC     = 0b100,
    O_NOBLOCK   = 0b1000,
    O_WRITE     = 0b10000,
    O_READ      = 0b100000,
};
// Alias to the Unix open flags
#define O_RDONLY O_READ
#define O_WRONLY O_WRITE
#define O_RDWR (O_RDONLY | O_WRONLY)
#define O_NONBLOCK O_NOBLOCK
#define O_NDELAY O_NOBLOCK
#define O_BINARY 0
typedef uint32_t oflags_t;
enum {
    SEEK_START,
    SEEK_CURSOR,
    SEEK_EOF,
};
typedef uintptr_t seekfrom_t;
typedef intptr_t off_t;

typedef struct {
    uint64_t size;
    ino_t inodeid;
    uint8_t kind;
    char name[];
} DirEntry;
