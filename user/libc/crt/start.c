#include <minos/sysstd.h>
#include <stdlib.h>
extern int main(int argc, const char** argv, int envc, const char** envv);
void _start(int argc, const char** argv, int envc, const char** envv) {
    _libc_internal_init_heap();
    exit(main(argc, argv, envc, envv));
}
