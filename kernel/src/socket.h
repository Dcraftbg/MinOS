#pragma once
#include <minos/socket.h>
#include <stddef.h>
#include <stdint.h>
typedef struct Inode Inode;
typedef struct {
    intptr_t (*init)(Inode* sock);
} SocketFamily;
extern SocketFamily socket_families[_AF_COUNT];
