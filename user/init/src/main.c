#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdexec.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Before fork....\n");
    setenv("PATH", "/user:", 0);
    intptr_t e = fork();
    const char* path = "/user/shell";
    if(e == (-YOU_ARE_CHILD)) {
        const char* argv[] = { path };
        if((e=execve(path, argv, sizeof(argv)/sizeof(*argv), (const char**)environ, __environ_size)) < 0) {
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
            printf("Child exited with: %d\n", (int)e);
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
    if((e = open("/devices/tty0", MODE_WRITE | MODE_READ, 0)) < 0) {
        exit(-e); 
    }
    _libc_internal_init_heap();
    _libc_init_environ(envv, envc);
    _libc_init_streams();
    fprintf(stderr, "\033[2J\033[H");
    printf("Args dump:\n");
    for(size_t i = 0; i < (size_t)argc; ++i) {
        printf("%zu> ",i+1); printf("%p",argv[i]); printf(" %s\n",argv[i]);
    }
    printf("Env dump:\n");
    for(size_t i = 0; i < (size_t)envc; ++i) {
        printf("%zu> ",i+1); printf("%p",envv[i]); printf(" %s\n",envv[i]);
    }
    int code = main();
    close(STDOUT_FILENO);
    if(STDIN_FILENO != STDOUT_FILENO) close(STDIN_FILENO);
    exit(code);
}
