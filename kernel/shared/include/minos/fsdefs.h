#pragma once
#include <stdint.h>
#include <stddef.h>
#include "stat.h"
#define PATH_MAX 4096
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
