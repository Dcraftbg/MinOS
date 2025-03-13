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
    MinOSClient *items;
    size_t len, cap;
    RwLock lock;
};
enum {
    MINOS_POOL_BACKLOGGED,
    MINOS_POOL_CONNECTED,
    MINOS_POOL_COUNT
};
struct MinOSServer {
    char addr[SOCKADDR_MINOS_PATH_MAX];
    MinOSConnectionPool pools[MINOS_POOL_COUNT];
};
typedef struct {
    uint8_t* addr;
    size_t len, cap;
} MinOSData;
struct MinOSClient {
    char addr[SOCKADDR_MINOS_PATH_MAX];
    MinOSData data;
    Mutex data_lock;
    MinOSServer* server;
    uint8_t server_pool; // Which pool are we in
    size_t server_idx; // Which connection in the list are we
    Mutex server_lock;
};
struct MinOSSocket {
    union {
        char addr[SOCKADDR_MINOS_PATH_MAX];
        MinOSClient client;
        MinOSServer server;
    };
};
intptr_t minos_socket_init(Socket* sock);
void minos_socket_init_cache(void);
