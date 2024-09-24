#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdbool.h>
#define HALT() \
    for(;;) 

const uintptr_t stdout = 0;
const uintptr_t stdin = 0;

#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "../vendor/stb_sprintf.h"
#include <minos/keycodes.h>

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
}
static bool isspace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
}
char* trim_r(char* buf) {
    char* start = buf;
    while(*buf) buf++;
    buf--;
    while(buf >= start && isspace(buf[0])) {
        *buf = '\0';
        buf--;
    }
    return start;
}
#define LINEBUF_MAX 1024
int main() {
    printf("Started MinOS shell\n");
    static char linebuf[LINEBUF_MAX];
    intptr_t e = 0;
    printf("> ");
    for(;;) {
        if((e=readline(linebuf, sizeof(linebuf)-1)) < 0) {
            printf("Failed to read on stdin: %s\n", status_str(e));
            return 1;
        }
        linebuf[e] = 0;
        const char* path = trim_r(linebuf);
        const char* argv[] = { path };
        if(strlen(path) != 0) {
            intptr_t e = fork();
            if(e == (-YOU_ARE_CHILD)) {
                if((e=exec(path, argv, sizeof(argv)/sizeof(*argv))) < 0) {
                    printf("ERROR: Failed to do exec: %s\n", status_str(e));
                    exit(1);
                }
                // Unreachable
                exit(0);
            } else if (e >= 0) {
                size_t pid = e;
                e=wait_pid(pid);
                if(e != 0) printf("%s exited with: %lu\n", path, e);
            } else {
                printf("ERROR: fork %s\n",status_str(e));
                exit(1);
            }
        }
        printf("> ");
    }
    return 0;
}
void _start(int argc, const char** argv) {
    exit(main());
}
