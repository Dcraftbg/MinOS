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

static intptr_t fb_init() {
    memset(&fbOps, 0, sizeof(fbOps));
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
