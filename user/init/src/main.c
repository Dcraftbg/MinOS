#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define HALT() \
    for(;;) 
#include <minos/keycodes.h>
typedef struct {
    uint16_t code    : 12;
    uint8_t  attribs : 4;
} Key;

intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    if((e = read(STDIN_FILENO, buf, bufmax)) < 0) {
        return e;
    }
    if(e > bufmax || buf[e-1] != '\n') return -BUFFER_OVEWFLOW;
    return e;
    /*
    size_t left = bufmax;
    while(left) {
        if((e=read(stdin, buf, 1)) < 0) {
            return e;
        }
        if(buf[0] == '\n') {
            return bufmax-left;
        }
        buf++;
        left--;
    }
    return -BUFFER_OVEWFLOW;
    */
}
#define LINEBUF_MAX 1024
int main() {
#if 0
    static char linebuf[LINEBUF_MAX];
    intptr_t e = 0;
    printf("Hello World!\n");
    printf("If you're seeing this it means that you successfully booted into user mode! :D\n");
    for(;;) {
        if((e=readline(linebuf, sizeof(linebuf)-1)) < 0) {
            printf("Failed to read on stdin: %s\n", status_str(e));
        }
        linebuf[e] = 0;
        printf("Read line: %s",linebuf);
    }
#endif
    printf("Before fork....\n");
    intptr_t e = fork();
    if(e == (-YOU_ARE_CHILD)) {
        const char* path = "/user/shell";
        const char* argv[] = { path };
        if((e=exec(path, argv, sizeof(argv)/sizeof(*argv))) < 0) {
            printf("ERROR: Failed to do exec: %s\n", status_str(e));
            exit(1);
        }
        // Unreachable
        exit(0);
    } else if (e >= 0) {
        size_t pid = e;
        e=wait_pid(pid);
        printf("Child exited with: %lu\n", e);
        exit(0);
    } else {
        printf("ERROR: fork %s\n",status_str(e));
        exit(1);
    }
    return 0;
}
void _start(int argc, const char** argv) {
    intptr_t e;
    if((e = open("/devices/tty0", MODE_WRITE | MODE_READ)) < 0) {
        HALT();
    }
    printf("Args dump:\n");
    for(size_t i = 0; i < argc; ++i) {
        printf("%zu> ",i+1); printf("%p",argv[i]); printf(" %s\n",argv[i]);
    }
    int code = main();
    close(STDOUT_FILENO);
    if(STDIN_FILENO != STDOUT_FILENO) close(STDIN_FILENO);
    exit(code);
}
