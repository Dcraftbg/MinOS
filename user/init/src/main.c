#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdbool.h>
#define HALT() \
    for(;;) 

uintptr_t stdout = 0;
uintptr_t stdin = 0;

#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "../vendor/stb_sprintf.h"
#include <minos/keycodes.h>
typedef struct {
    uint16_t code    : 12;
    uint8_t  attribs : 4;
} Key;


#define PRINTF_TMP 1024
static char tmp_printf[PRINTF_TMP]={0};

size_t strlen(const char* cstr) {
    const char* head = cstr;
    while(*head) head++;
    return head-cstr;
}
void printf(const char* fmt, ...) __attribute__((format(printf,1, 2)));
void printf(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    size_t n = stbsp_vsnprintf(tmp_printf, PRINTF_TMP, fmt, va);
    va_end(va);
    write(stdout, tmp_printf, n);
}
intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    if((e = read(stdin, buf, bufmax)) < 0) {
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
    stdout = e;
    stdin = e;
    printf("Args dump:\n");
    for(size_t i = 0; i < argc; ++i) {
        printf("%zu> ",i+1); printf("%p",argv[i]); printf(" %s\n",argv[i]);
    }
    int code = main();
    close(stdout);
    if(stdin != stdout) close(stdin);
    exit(code);
}
