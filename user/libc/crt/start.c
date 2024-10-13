#include <minos/sysstd.h>
extern int main(int argc, const char** argv);
void _start(int argc, const char** argv) {
    exit(main(argc, argv));
}
