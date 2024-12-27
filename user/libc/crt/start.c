#include <minos/sysstd.h>
#include <stdlib.h>
#include <environ.h>
#include <stdio.h>
extern int main(int argc, const char** argv, int envc, const char** envv);
void _start(int argc, const char** argv, int envc, const char** envv) {
    asm volatile("push %rbp");
    _libc_internal_init_heap();
    _libc_init_environ(envv, envc);
    _libc_init_streams();
    exit(main(argc, argv, envc, envv));
}
