#pragma once
#define AF_UNSPEC 0
#define AF_MINOS  1
#define _AF_COUNT 2
#define SOCKADDR_MINOS_PATH_MAX 128
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#include <stddef.h>
#include <stdint.h>
typedef uint32_t sa_family_t;
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
};
struct sockaddr_minos {
    sa_family_t sminos_family; /* AF_MINOS */
    char sminos_path[SOCKADDR_MINOS_PATH_MAX];
    /* only used for client address */
    size_t sminos_pid;
};
