#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdexec.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    if((e = read(STDIN_FILENO, buf, bufmax)) < 0) {
        return e;
    }
    if(e > bufmax || buf[e-1] != '\n') return -BUFFER_OVEWFLOW;
    return e;
}
#define LINEBUF_MAX 1024
int main() {
    printf("Before fork....\n");
    intptr_t e = fork();
    const char* path = "/user/shell";
    if(e == (-YOU_ARE_CHILD)) {
        const char* argv[] = { path };
        if((e=exec(path, argv, sizeof(argv)/sizeof(*argv))) < 0) {
            printf("ERROR: Failed to do exec: %s\n", status_str(e));
            exit(-e);
        }
        // Unreachable
        exit(0);
    } else if (e >= 0) {
        size_t pid = e;
        e=wait_pid(pid);
        if(e == NOT_FOUND) {
            printf("Could not find command `%s`\n", path);
        } else {
            printf("Child exited with: %lu\n", e);
        }
        exit(1);
    } else {
        printf("ERROR: fork %s\n",status_str(e));
        exit(1);
    }
    return 0;
}
void _start(int argc, const char** argv, int envc, const char** envv) {
    intptr_t e;
    if((e = open("/devices/tty0", MODE_WRITE | MODE_READ)) < 0) {
        exit(-e); 
    }
    printf("Args dump:\n");
    for(size_t i = 0; i < argc; ++i) {
        printf("%zu> ",i+1); printf("%p",argv[i]); printf(" %s\n",argv[i]);
    }
    printf("Env dump:\n");
    for(size_t i = 0; i < envc; ++i) {
        printf("%zu> ",i+1); printf("%p",envv[i]); printf(" %s\n",envv[i]);
    }
    environ=NULL;
    __environ_size=0;
    int code = main();
    close(STDOUT_FILENO);
    if(STDIN_FILENO != STDOUT_FILENO) close(STDIN_FILENO);
    exit(code);
}
