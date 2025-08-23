#pragma once
#include <stdint.h>
typedef uint64_t ino_t;
struct statx {
    uint64_t stx_mask;
    ino_t    stx_ino;
    uint64_t stx_size;
    uint8_t  stx_type;
};
#define STATX_INO  (1 << 0)
#define STATX_SIZE (1 << 1)
#define STATX_TYPE (1 << 2)
enum {
    STX_TYPE_DIR,
    STX_TYPE_FILE,
    STX_TYPE_DEVICE,
    STX_TYPE_MINOS_SOCKET,
    STX_TYPE_EPOLL,
    STX_TYPE_COUNT,
};
