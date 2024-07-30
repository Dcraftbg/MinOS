#include "vga.h"
#include "../../fonts/zap-light16.h"
static FsOps vgaOps = {0};
#if 0
static intptr_t vga_open(struct Device* this, VfsFile* file, fmode_t mode) {
     if(mode != MODE_WRITE) return -UNSUPPORTED;
     file->private = this->private;
     return 0;
}
static intptr_t vga_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
     (void)offset;
     (void)buf;
     (void)size;
     (void)offset;
     return 0;
}
#endif
typedef struct {
    Framebuffer fm;
    size_t x, y;
} VgaDevice;
static Cache* vga_cache = NULL;

static intptr_t vga_open(Device* this, VfsFile* file, fmode_t mode) {
    if(mode != MODE_WRITE) return -UNSUPPORTED;
    file->private = this->private;
    return 0;
}
static void vga_close(VfsFile* file) {
    file->private = NULL;
}
static void delete_vga_private(VgaDevice* device) {
    cache_dealloc(vga_cache, device);
}
static intptr_t new_vga_private(void** private, size_t id) {
    debug_assert(private);
    VgaDevice* vga = cache_alloc(vga_cache);
    if(vga == NULL) return -NOT_ENOUGH_MEM;
    memset(vga, 0, sizeof(*vga));
    *private = vga;
    vga->fm = get_framebuffer_by_id(id);
    if(!vga->fm.addr) {
       cache_dealloc(vga_cache, vga);
       return -NOT_FOUND;
    }
    if(vga->fm.bpp != 32) {
       cache_dealloc(vga_cache, vga);
       return -UNSUPPORTED;
    }
    return 0;
}
// TODO: Framebuffer optimisations
static intptr_t vga_draw_codepoint_at(Framebuffer* fm, size_t x, size_t y, int codepoint, uint32_t color) {
    if(codepoint > 127) return -UNSUPPORTED; // Unsupported
    uint8_t* fontPtr = fontGlythBuffer + (codepoint*fontHeader.charsize);
    debug_assert(fontPtr + 16 < fontGlythBuffer+ARRAY_LEN(fontGlythBuffer));
    for(size_t cy = y; cy < y+16 && cy < fm->height; ++cy) {
        for(size_t cx = x; cx < x+8 && cx < fm->width; ++cx) {
            if((*fontPtr & (0b10000000 >> (cx-x))) > 0) {
                fmbuf_set_at(fm, cx, cy, color);
            }
        }
        fontPtr++;
    }
    return 0;
}
static intptr_t vga_draw_codepoint(VgaDevice* device, int codepoint, uint32_t color) {
    if(device->y >= device->fm.height) return 0;
    switch(codepoint) {
       case '\n':
         device->x=0;
         device->y+=16;
         break;
       case '\t': {
         size_t w = 8 * 4;
         if(device->x+w >= device->fm.width) {
            device->y += 16;
            device->x = 0;
         }
         device->x += w;
       } break;
       case '\r':
         device->x = 0;
         break;
       case ' ':
         if (device->x+8 >= device->fm.width) {
             device->y += 16;
             device->x = 0;
         }
         device->x += 8;
         break;
       default: {
         if(device->x+8 >= device->fm.width) {
             device->y += 16;
             device->x = 0;
         }
         intptr_t e = vga_draw_codepoint_at(&device->fm, device->x, device->y, codepoint, color);
         if(e >= 0) device->x += 8;
         return e;
       }
    }
    return 0;
}
#define VGA_COLOR 0xFFFFFFFF
static intptr_t vga_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
    (void)offset;
    intptr_t e = 0;
    VgaDevice* vga = (VgaDevice*)file->private;
    for(size_t i = 0; i < size; ++i) {
        uint8_t codepoint = ((const char*)buf)[i];
        if((e = vga_draw_codepoint(vga, codepoint, VGA_COLOR)) < 0) return e;
    }
    return size;
}
void destroy_vga_device(Device* device) {
    if(!device) return;
    if(!device->private) return;
    delete_vga_private((VgaDevice*)device->private);
    device->private = NULL;
}
intptr_t vga_init();
intptr_t create_vga_device(size_t id, Device* device) {
    static_assert(sizeof(Device) == 40, "Update create_vga_device");
    intptr_t e = 0;
    device->ops = &vgaOps;
    device->open = vga_open;
    device->init = vga_init;
    device->deinit = NULL;
    if((e = new_vga_private(&device->private, id)) < 0) return e;
    return 0;
}
intptr_t vga_init() {
    memset(&vgaOps, 0, sizeof(vgaOps));
    vgaOps.write = vga_write;
    vgaOps.close = vga_close;
    vga_cache = create_new_cache(sizeof(VgaDevice));
    if(!vga_cache) return -NOT_ENOUGH_MEM;
    return 0;
}