#include "blocking.h"
#include "log.h"
#include <assert.h>
#include <minos/status.h>
#include <stdexec.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#define NUM_CONNECTIONS 2
static intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    size_t n = 0;
    while(n < bufmax) {
        e = read(STDIN_FILENO, buf + n, bufmax - n);
        if(e < 0) return e;
        assert(e != 0);
        n += e;
        if(buf[n-1] == '\n') break;
    }
    if(n >= bufmax) return -BUFFER_TOO_SMALL;
    return n;
}
int blocking_server(void*) {
    log_where = "server (blocking)";
    int fd = socket(AF_MINOS, SOCK_STREAM, 0);
    if(fd < 0) {
        fprintf(stderr, "ERROR: Failed to create socket: %s\n", strerror(errno));
        return 1;
    }
    info("Created socket: %d", fd);
    struct sockaddr_minos minos;
    minos.sminos_family = AF_MINOS;
    const char* addr = "/sockets/gtnet";
    strcpy(minos.sminos_path, addr);
    if(bind(fd, (struct sockaddr*)&minos, sizeof(minos)) < 0) {
        error("Failed to create bind socket `%s`: %s", addr, strerror(errno));
        return 1;
    }
    info("Bound socket");
    if(listen(fd, 50) < 0) {
        error("Failed to listen on socket: %s", strerror(errno));
        return 1;
    }
    info("Waiting connections...");
    for(size_t i = 0; i < NUM_CONNECTIONS; ++i) {
        int client = accept(fd, NULL, NULL);
        if(client < 0) {
            error("Failed to accept on socket: %s", strerror(errno));
            return 1;
        }
        info("accepted client...");
        info("awaiting message...");
        char buf[128];
        ssize_t e = recv(client, buf, sizeof(buf)-1, 0);
        if(e < 0) {
            warn("Failed to recv on socket: %s", strerror(errno));
            continue;
        }
        buf[(size_t)e] = '\0';
        info("Received message: `%s`", buf);
        info("sending back message...");
        send(client, buf, (size_t)e, 0);
        info("closing client...");
        close(client);
    }
    info("closing...");
    close(fd);
    return 0;
}
int blocking_client(void*) {
    log_where = "client (blocking)";
    int fd = socket(AF_MINOS, SOCK_STREAM, 0);
    if(fd < 0) {
        error("Failed to create socket: %s", strerror(errno));
        return 1;
    }
    struct sockaddr_minos server_addr;
    server_addr.sminos_family = AF_MINOS;
    const char* addr = "/sockets/gtnet";
    Stats _stats;
    intptr_t e;
    while((e=stat(addr, &_stats)) < 0 && e == -NOT_FOUND);
    // info("Trying to connect...");
    strcpy(server_addr.sminos_path, addr);
    if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        error("Failed to connect to %s: %s", addr, strerror(errno));
        return 1;
    }
    // info("Calling send...");
    char msg[128];
    intptr_t n;
    assert((n=readline(msg, sizeof(msg)-1)) >= 0);
    send(fd, msg, n, 0);
    n = recv(fd, msg, sizeof(msg)-1, 0);
    assert(n >= 0);
    msg[n] = '\0';
    printf("%s", msg);
    // info("quitting server");
    close(fd);
    return 0;
}
