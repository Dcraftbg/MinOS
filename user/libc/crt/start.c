#include <minos/sysstd.h>
#include <stdlib.h>
extern int main(int argc, const char** argv);
void _start(int argc, const char** argv) {
    _libc_internal_init_heap();
    exit(main(argc, argv));
}
