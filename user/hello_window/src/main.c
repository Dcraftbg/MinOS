#include <stdio.h>
#include <sys/socket.h>
#include <minos/sysstd.h>
#include <minos/status.h>
#include <string.h>
#include <errno.h>
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
#if 0
static intptr_t read_u32(uintptr_t fd, uint32_t* data) {
    return read_exact(fd, data, sizeof(*data));
}
static intptr_t read_u16(uintptr_t fd, uint16_t* data) {
    return read_exact(fd, data, sizeof(*data));
}
static intptr_t read_u8(uintptr_t fd, uint8_t* data) {
    return read_exact(fd, data, sizeof(*data));
}
#endif

static intptr_t write_u32(uintptr_t fd, uint32_t data) {
    return write_exact(fd, &data, sizeof(data));
}
static intptr_t write_u16(uintptr_t fd, uint16_t data) {
    return write_exact(fd, &data, sizeof(data));
}
static intptr_t write_u8(uintptr_t fd, uint8_t data) {
    return write_exact(fd, &data, sizeof(data));
}
static intptr_t write_i32(uintptr_t fd,  int32_t data) {
    return write_u32(fd, (uint32_t)data);
}

static void sleep_milis(size_t milis) {
    MinOS_Duration duration;
    size_t secs = milis / 1000, rem = milis % 1000;
    duration.secs = secs;
    duration.nano = rem * 1000000;
    sleepfor(&duration);
}
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
    write_packet(wm, 40 + 12, 0x01);
    write_i32(wm, -1);  // x
    write_i32(wm, -1);  // y
    write_u32(wm, 500); // w
    write_u32(wm, 500); // h
    write_i32(wm, -1);  // min w
    write_i32(wm, -1);  // min h
    write_i32(wm, -1);  // max w
    write_i32(wm, -1);  // max h
    write_u32(wm, 0);   // flags
    const char* title = "Hello Window";
    write_u32(wm, strlen(title)); // title length
    write_exact(wm, title, strlen(title)); // title
    for(size_t i = 0; i < 16; ++i) {
        fprintf(stderr, "Sending test packet (1 sec between packets)\n");
        write_packet(wm, 0, 0x69);
        sleep_milis(1000);
    }
    for(;;);
    close(wm);
    return 0;
}
