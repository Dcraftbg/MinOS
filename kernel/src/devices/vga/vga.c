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
    fmbuf_fill(&vga->fm, VGA_BG);
    return 0;
}
intptr_t vga_draw_codepoint_at(Framebuffer* fm, size_t x, size_t y, int codepoint, uint32_t fg, uint32_t bg) {
    if(codepoint > 127) return -UNSUPPORTED; // Unsupported
    if(fm->bpp != 32) return -UNSUPPORTED; // Because of optimisations we don't support anything besides 32 bits per pixel
    uint8_t* fontPtr = fontGlythBuffer + (codepoint*fontHeader.charsize);
    debug_assert(fontPtr + 16 < fontGlythBuffer+ARRAY_LEN(fontGlythBuffer));
    uint32_t* strip = (uint32_t*)(((uint8_t*)fm->addr) + fm->pitch_bytes*y);
    for(size_t cy = y; cy < y+16 && cy < fm->height; ++cy) {
        for(size_t cx = x; cx < x+8 && cx < fm->width; ++cx) {
            strip[cx] = ((*fontPtr & (0b10000000 >> (cx-x))) > 0) ? fg : bg;
        }
        fontPtr++;
        strip = (uint32_t*)((uint8_t*)strip + fm->pitch_bytes);
    }
    return 0;
}
static intptr_t vga_draw_codepoint(VgaDevice* device, int codepoint, uint32_t fg, uint32_t bg) {
    if(device->y >= device->fm.height) return 0;
    switch(codepoint) {
       case '\n':
         device->x=0;
         device->y+=16;
         break;
       case '\t': {
         size_t w = 8 * 4;
         fmbuf_draw_rect(&device->fm, device->x, device->y, device->x+w, device->y+16, bg);
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
         fmbuf_draw_rect(&device->fm, device->x, device->y, device->x+8, device->y+16, bg);
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
         intptr_t e = vga_draw_codepoint_at(&device->fm, device->x, device->y, codepoint, fg, bg);
         if(e >= 0) device->x += 8;
         return e;
       }
    }
    return 0;
}
static intptr_t vga_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
    (void)offset;
    intptr_t e = 0;
    VgaDevice* vga = (VgaDevice*)file->private;
    for(size_t i = 0; i < size; ++i) {
        uint8_t codepoint = ((const char*)buf)[i];
        if((e = vga_draw_codepoint(vga, codepoint, VGA_FG, VGA_BG)) < 0) return e;
    }
    return size;
}
static intptr_t vga_stat(Device* this, VfsStats* stats) {
    if(!this || !this->private || !stats) return -INVALID_PARAM;
    VgaDevice* vga = this->private;
    memset(stats, 0, sizeof(*stats));
    stats->lba = 0;
    stats->width  = vga->fm.width  / 8;
    stats->height = vga->fm.height / 16;
    return 0;
}
void destroy_vga_device(Device* device) {
    if(!device) return;
    if(!device->private) return;
    delete_vga_private((VgaDevice*)device->private);
    device->private = NULL;
}
intptr_t create_vga_device(size_t id, Device* device) {
    static_assert(sizeof(Device) == 32, "Update create_vga_device");
    intptr_t e = 0;
    device->ops = &vgaOps;
    device->open = vga_open;
    device->stat = vga_stat;
    if((e = new_vga_private(&device->private, id)) < 0) return e;
    return 0;
}
intptr_t vga_init() {
    memset(&vgaOps, 0, sizeof(vgaOps));
    vgaOps.write = vga_write;
    vgaOps.close = vga_close;
    vga_cache = create_new_cache(sizeof(VgaDevice), "VgaDevice");
    if(!vga_cache) return -NOT_ENOUGH_MEM;
    return 0;
}
