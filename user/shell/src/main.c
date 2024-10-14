#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    size_t i = 0;
    while(i < bufmax) {
        if((e = read(STDIN_FILENO, buf+i, 1)) < 0) {
            return e;
        }
        if(buf[i++] == '\n') return i;
    }
    return -BUFFER_OVEWFLOW;
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
                    exit(-e);
                }
                // Unreachable
                exit(0);
            } else if (e >= 0) {
                size_t pid = e;
                e=wait_pid(pid);
                if(e != 0) {
                   if(e == NOT_FOUND) {
                       printf("Could not find command `%s`\n", path);
                   } else {
                       printf("%s exited with: %lu\n", path, e);
                   }
                }
            } else {
                printf("ERROR: fork %s\n",status_str(e));
                exit(1);
            }
        }
        printf("> ");
    }
    return 0;
}
