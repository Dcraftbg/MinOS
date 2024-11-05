#pragma once
enum {
    MODE_READ =0b01,
    MODE_WRITE=0b10,
    /*append?*/
};
typedef uint32_t fmode_t;
#define PATH_MAX 4096
enum {
    O_CREAT=0b1,
};
typedef uint32_t oflags_t;

enum {
    SEEK_START,
    SEEK_CURSOR,
    SEEK_EOF,
};
typedef uintptr_t seekfrom_t;
typedef intptr_t off_t;
