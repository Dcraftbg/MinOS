#include <minos/sysstd.h>
#include <minos/fb/fb.h>
#include <minos/status.h>
#include <stdio.h>
static int mini(int a, int b) {
    return a < b ? a : b;
}
int main() {
    intptr_t e;
    uintptr_t fb;
    const char* fbpath = "/devices/fb0";
    if((e=open(fbpath, MODE_READ | MODE_WRITE, 0)) < 0) {
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
    uint32_t* pixels;
    if((e=mmap(fb, (void**)&pixels, 0)) < 0) {
        fprintf(stderr, "ERROR: Failed to mmap on fb: %s\n", status_str(e));
        close(fb);
        return 1;
    }
    size_t end = mini(mini(stats.width, stats.height), 100);

    for(size_t y = 0; y < end; ++y) {
        pixels[y] = 0xFF0000;
        pixels = (uint32_t*)(((uint8_t*)pixels) + stats.pitch_bytes);
    }
    close(fb);
    return 0;
}
