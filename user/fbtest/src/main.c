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
    FbStats stats={0};
    if(fbget_stats(fb, &stats) < 0) {
        fprintf(stderr, "ERROR: Failed to get stats on fb: %s\n", status_str(e));
        close(fb);
        return 1;
    }
    printf("Framebuffer is %zux%zu pixels (%zu bits per pixel)\n", (size_t)stats.width, (size_t)stats.height, (size_t)stats.bpp);
    close(fb);
    return 0;
}
