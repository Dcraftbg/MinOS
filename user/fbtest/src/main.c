#include <minos/sysstd.h>
#include <minos/fb/fb.h>
#include <minos/status.h>
#include <stdio.h>
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

intptr_t read_exact(uintptr_t file, void* bytes, size_t amount) {
    while(amount) {
        ssize_t rb = read(file, bytes, amount);
        if(rb < 0) return rb;
        if(rb == 0) return -PREMATURE_EOF;
        amount-=(size_t)rb;
        bytes+=(size_t)rb;
    }
    return 0;
}
typedef struct {
    char* data;
    size_t len;
} Buf;
intptr_t load(const char* path, Buf* res) {
    intptr_t e;
    uintptr_t fd;
    if((e=open(path, MODE_READ, 0)) < 0) {
        fprintf(stderr, "Failed to open `%s`: %s\n", path, status_str(e));
        return e;
    }
    fd=e;
    if((e=seek(fd, 0, SEEK_EOF)) < 0) {
        fprintf(stderr, "Failed to seek: %s\n", status_str(e));
        goto err_seek;
    }
    size_t size = e;
    if((e=seek(fd, 0, SEEK_START)) < 0) {
        fprintf(stderr, "Failed to seek back: %s\n", status_str(e));
        goto err_seek;
    }
    char* buf = malloc(size);
    if(!buf) {
        fprintf(stderr, "Failed: Not enough memory :(\n");
        goto err;
    }
    if((e=read_exact(fd, buf, size)) < 0) {
        fprintf(stderr, "Failed to read: %s\n", status_str(e));
        goto err;
    }
    res->data = buf;
    res->len = size;
    close(fd);
    return 0;
err:
    free(buf);
err_seek:
    close(fd);
    return e;
}
uint32_t abgr_to_argb(uint32_t a) {
    uint8_t alpha = (a >> 24) & 0xFF; 
    uint8_t blue = (a >> 16) & 0xFF;   
    uint8_t green = (a >> 8) & 0xFF; 
    uint8_t red = a & 0xFF;            
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}
const char* shift_args(int *argc, const char ***argv) {
    if((*argc) <= 0) return NULL;
    return ((*argc)--, *((*argv)++));
}
int main(int argc, const char** argv) {
    intptr_t e;
    uintptr_t fb;
    const char* fbpath = "/devices/fb0";
    const char* imgpath = "/VulkanMeme.jpg";

    // eat the executable path
    shift_args(&argc, &argv);
    if(argc > 0) imgpath = shift_args(&argc, &argv);
    Buf img_buf;
    if((e=load(imgpath, &img_buf)) < 0) {
        fprintf(stderr, "ERROR: Failed to load `%s`\n", imgpath);
        return 1;
    }
    int w, h;
    uint32_t* img = (uint32_t*)stbi_load_from_memory((void*)img_buf.data, img_buf.len, &w, &h, NULL, 4);
    if(!img) {
        fprintf(stderr, "Failed to load `%s` from memory :(\n", imgpath);
        free(img_buf.data);
        return 1;
    }
    if((e=open(fbpath, MODE_READ | MODE_WRITE, 0)) < 0) {
        fprintf(stderr, "ERROR: Failed to open %s: %s\n", fbpath, status_str(e));
        goto err_fb;
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
    uint8_t* head=(uint8_t*)pixels;
    
    for(size_t y=0; y < stats.height; ++y) {
       for(size_t x=0; x < stats.width; ++x) {
           ((uint32_t*)head)[x] = abgr_to_argb(img[(y%h)*w+(x%w)]);
       }
       head += stats.pitch_bytes;
    }
    close(fb);
    return 0;
err_fb:
    free(img);
    return 1;
}
