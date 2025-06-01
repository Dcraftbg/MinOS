#include <stdio.h>
#include <sys/socket.h>
#include <minos/sysstd.h>
#include <minos/status.h>
#include <string.h>
#include <errno.h>
#include <libwm.h>
#include <libwm/tags.h>

#define WM_PATH "/sockets/wm"
static int minos_connectto(const char* addr) {
    int fd = socket(AF_MINOS, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("socket");
        return -errno;
    }
    struct sockaddr_minos server_addr;
    server_addr.sminos_family = AF_MINOS;
    strcpy(server_addr.sminos_path, addr);
    if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "connect %s: %s\n", addr, strerror(errno));
        close(fd);
        return -errno;
    }
    return fd;
}
static intptr_t write_exact(int fd, const void* buf, size_t size) {
    intptr_t e;
    while(size) {
        e = write(fd, buf, size);
        if(e < 0) return e;
        if(e == 0) return -PREMATURE_EOF;
        size -= (size_t)e;
        buf += e;
    }
    return 0;
}
static intptr_t write_packet(int fd, uint32_t payload_size, uint16_t tag) {
    intptr_t e;
    payload_size += sizeof(tag); // <- We need to include tag into the payload_size
    if((e=write_exact(fd, &payload_size, sizeof(payload_size))) < 0) return e;
    if((e=write_exact(fd, &tag, sizeof(tag))) < 0) return e;
    return 0;
}
static void sleep_milis(size_t milis) {
    MinOS_Duration duration;
    size_t secs = milis / 1000, rem = milis % 1000;
    duration.secs = secs;
    duration.nano = rem * 1000000;
    sleepfor(&duration);
}
#define wmwrite ((ssize_t (*)(void*, const void*, size_t)) write)
int main(void) {
    printf("Hello World!\n");
    printf("Connecting...\n");
    int wm = minos_connectto(WM_PATH);
    if(wm < 0) {
        perror("minos_connectto(" WM_PATH ")");
        return 1;
    }
    printf("Connected!\n");
    printf("Creating window!\n");

    WmCreateWindowInfo info = {
        .x = 0,
        .y = 0,
        .width = 500,
        .height = 500,
        .min_width = 500,
        .min_height = 500,
        .max_width = 500,
        .max_height = 500,
        .flags = 0,
        .title = "Hello bro",
    };
    info.title_len = strlen(info.title);
    write_packet(wm, size_WmCreateWindowInfo(&info), WM_PACKET_TAG_CREATE_WINDOW);
    write_WmCreateWindowInfo((void*)(uintptr_t)wm, wmwrite, &info);
    for(;;);
    close(wm);
    return 0;
}
