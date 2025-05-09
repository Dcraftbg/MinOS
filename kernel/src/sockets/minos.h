#pragma once
#include <sync/mutex.h>
#include <sync/rwlock.h>
#include <stddef.h>
#include <stdint.h>
#include <socket.h>
typedef struct MinOSSocket MinOSSocket;
typedef struct MinOSServer MinOSServer;
typedef struct MinOSClient MinOSClient;
typedef struct MinOSConnectionPool MinOSConnectionPool;
struct MinOSConnectionPool {
    MinOSClient **items;
    size_t len, cap;
    RwLock lock;
};
enum {
    MINOS_POOL_BACKLOGGED,
    MINOS_POOL_COUNT
};
#define MINOS_SOCKET_MAX_CONNECTIONS ((4*PAGE_SIZE)/sizeof(MinOSClient*))
#define MINOS_SOCKET_MAX_DATABUF     (4*PAGE_SIZE)
struct MinOSServer {
    char addr[SOCKADDR_MINOS_PATH_MAX];
    MinOSConnectionPool pending;
};
typedef struct {
    uint8_t* addr;
    size_t len, cap;
} MinOSData;
intptr_t minos_data_reserve_at_least(MinOSData* dp, size_t n);
void minos_data_free(MinOSData* dp);
struct MinOSClient {
    char addr[SOCKADDR_MINOS_PATH_MAX];
    atomic_bool closed, pending; 
    MinOSData client_write_buf;
    Mutex client_write_buf_lock;

    MinOSData client_read_buf;
    Mutex client_read_buf_lock;
};
struct MinOSSocket {
    union {
        struct {
            char addr[SOCKADDR_MINOS_PATH_MAX];
        };
        MinOSClient client;
        MinOSServer server;
    };
};
intptr_t minos_socket_init(Inode* sock);
void minos_socket_init_cache(void);
