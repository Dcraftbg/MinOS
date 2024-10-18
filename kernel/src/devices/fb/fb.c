#include "fb.h"
#include "../../log.h"
#include <minos/fb/fb.h>
#include "../../kernel.h"
// GLOBALS
Cache* fbd_cache=NULL;
static FsOps fbOps = {0};
// TODO: Some sort of synchronization for Framebuffers like with inodes. Maybe TtyFb should borrow Framebuffer or something
typedef struct {
    Framebuffer fb;
} FbDevice;
void destroy_fb_device(Device* device) {
    if(!device || !device->private) return;
    FbDevice* fbd = device->private;
    cache_dealloc(fbd_cache, fbd);
}

static intptr_t fb_get_stats(FbDevice* device, FbStats* stats) {
    stats->width = device->fb.width;
    stats->height = device->fb.height;
    stats->bpp = device->fb.bpp;
    return 0;
}
// TODO: Maybe do an ioctl function map. IDRK
static intptr_t fb_ioctl(VfsFile* file, Iop op, void* arg) {
    if(!file || !file->private) return -INVALID_PARAM;
    FbDevice* device = (FbDevice*)file->private;
    switch(op) {
    case FB_IOCTL_GET_STATS: return fb_get_stats(device, (FbStats*)arg);
    }
    return -INVALID_PARAM;
}

static intptr_t fb_open(struct Device* this, VfsFile* file, fmode_t mode) {
    file->private = this->private;
    return 0;
}
static void fb_close(VfsFile* file) {
    file->private = NULL;
}

static intptr_t fb_init() {
    memset(&fbOps, 0, sizeof(fbOps));
    fbOps.ioctl = fb_ioctl;
    fbOps.close = fb_close;
    fbd_cache = create_new_cache(sizeof(FbDevice), "FbDevice");
    if(!fbd_cache) return -NOT_ENOUGH_MEM;
    return 0;
}
intptr_t init_fb_devices() {
    intptr_t e;
    if((e=fb_init()) < 0) return e;
    size_t count = get_framebuffer_count();
    char namebuf[128];
    for(size_t i = 0; i < count; ++i) {
        Device* device = (Device*)cache_alloc(kernel.device_cache);
        if(!device) {
            return -NOT_ENOUGH_MEM;
        }
        FbDevice* fbd = (FbDevice*)cache_alloc(fbd_cache);
        if(!fbd) {
            e = -NOT_ENOUGH_MEM;
            goto err_null_fb_dev;
        }
        fbd->fb = get_framebuffer_by_id(i);
        if(!fbd->fb.addr) goto err_invalid_framebuffer;
        device->private = fbd;
        device->ops = &fbOps;
        device->open = fb_open;
        stbsp_snprintf(namebuf, sizeof(namebuf), "fb%zu", i);
        if((e=vfs_register_device(namebuf, device))) goto err_register_device;
        continue;
err_register_device:
err_invalid_framebuffer:
        cache_dealloc(fbd_cache, fbd);
err_null_fb_dev:
        cache_dealloc(kernel.device_cache, device);
        if(e) return e;
    }
    return 0;
}
