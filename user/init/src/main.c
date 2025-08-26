#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <minos/sysstd.h>

int main() {
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
    setenv("PATH", "/user:", 0);
    intptr_t e = fork();
    const char* path = "/user/shell";
    if(e == 0) {
        const char* argv[] = { path, NULL };
        if(execve(path, (char*const*) argv, (char*const*)environ) < 0) printf("ERROR: Failed to do exec: %s\n", strerror(errno));
        return errno;
    } else if (e >= 0) {
        size_t pid = e;
        e = wait_pid(pid);
        if(e == ENOENT) {
            printf("Could not find command `%s`\n", path);
        } else {
            printf("Child exited with: %d\n", (int)e);
        }
        exit(1);
    } else {
        printf("ERROR: fork %s\n", strerror(errno));
        exit(1);
    }
    return 0;
}

void __libc_start_main(int argc, const char** argv, const char** envp, void** reserved, ...);
void _start(int argc, const char** argv, const char** envp, void** reserved) {
    if(open("/devices/tty0", O_RDWR) < 0) exit(1); 
    __libc_start_main(argc, argv, envp, reserved, main);
}
