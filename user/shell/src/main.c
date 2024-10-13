#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdbool.h>
#include <stdio.h>
#define HALT() \
    for(;;) 

#include <string.h>

intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    if((e = read(STDIN_FILENO, buf, bufmax)) < 0) {
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
