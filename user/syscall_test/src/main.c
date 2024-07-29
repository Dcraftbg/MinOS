#include "sysstd.h"
#define HALT() \
    for(;;) 
void _start() {
    uintptr_t stdout = 0;
    if((stdout = open("/devices/vga0", MODE_WRITE)) < 0) HALT();
    write(stdout, "Hello World!\n", 13);
    write(stdout, "If you're seeing this it means that you successfully booted into user mode! :D\n", 79);
    close(stdout);
    HALT();
}
