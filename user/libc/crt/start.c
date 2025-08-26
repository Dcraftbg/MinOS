#include <minos/sysstd.h>
#include <stdlib.h>
#include <environ.h>
#include <stdio.h>
extern int main(int argc, const char** argv, const char** envp);
void _start(int argc, const char** argv, const char** envp) {
    asm volatile("push %rbp");
    _libc_init_environ(envp);
    _libc_init_streams();
    exit(main(argc, argv, envp));
}
