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
