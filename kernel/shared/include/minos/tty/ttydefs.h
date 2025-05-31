#pragma once
enum {
    TTY_ECHO    = 0b1,
    TTY_INSTANT = 0b10,
};
enum {
    TTY_IOCTL_SET_FLAGS=1,
    TTY_IOCTL_GET_FLAGS,
    TTY_IOCTL_GET_SIZE,
    TTY_IOCTL_SET_SIZE,
};
typedef uint32_t ttyflags_t;
typedef struct {
    uint32_t width, height;
} TtySize;
