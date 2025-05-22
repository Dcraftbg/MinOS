#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <minos/sysstd.h>
#include <minos/status.h>
#include <assert.h>
#include "log.h"
#include "blocking.h"
#define GT_IMPLEMENTATION
#include "gt.h"

const char* shift_args(int *argc, const char ***argv) {
    if((*argc) <= 0) return NULL;
    return ((*argc)--, *((*argv)++));
}
#include <assert.h>
#include <minos/status.h>
#include <stdexec.h>
void server_client(void* client_fd_void) {
    int client = (uintptr_t)client_fd_void;
    info("awaiting message...");
    char buf[128];
    gtblockfd(client, GTBLOCKIN);
    ssize_t e = recv(client, buf, sizeof(buf)-1, 0);
    if(e < 0) {
        warn("Failed to recv on socket: %s", strerror(errno));
        return;
    }
    buf[(size_t)e] = '\0';
    info("Received message: `%s`", buf);
    info("sending back message...");
    gtblockfd(client, GTBLOCKOUT);
    send(client, buf, (size_t)e, 0);
    info("closing client...");
    close(client);
}
int server(void) {
    gtinit();
    log_where = "server";
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
    for(;;) {
        gtblockfd(fd, GTBLOCKIN);
        int client = accept(fd, NULL, NULL);
        if(client < 0) {
            error("Failed to accept on socket: %s", strerror(errno));
            return 1;
        }
        gtgo(server_client, (void*)((uintptr_t)client));
    }
    return 0;
}
int main(int argc, const char** argv) {
    const char* exe = shift_args(&argc, &argv);
    (void)exe;
    if(argc <= 0) {
        fprintf(stderr, "ERROR: Missing subcommand! (bserver|server|client)\n");
        return 1;
    }
    const char* cmd = shift_args(&argc, &argv);
    if(strcmp(cmd, "bserver") == 0) {
        blocking_server(NULL);
    } else if (strcmp(cmd, "client") == 0) {
        blocking_client(0);
    } else if (strcmp(cmd, "server") == 0) {
        server();
    }
    return 0;
}
