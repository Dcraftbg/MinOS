#include <minos/sysstd.h>
#include <minos/fb/fb.h>
#include <minos/status.h>
#include <stdio.h>
int main() {
    intptr_t e;
    uintptr_t fb;
    const char* fbpath = "/devices/fb0";
    if((e=open(fbpath, MODE_READ | MODE_WRITE)) < 0) {
        fprintf(stderr, "ERROR: Failed to open %s: %s\n", fbpath, status_str(e));
        return 1;
    }
    fb=e;
    close(fb);
    return 0;
}
