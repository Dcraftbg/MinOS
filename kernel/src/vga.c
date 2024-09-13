#include "vga.h"
#include "bootutils.h"
#include "kernel.h"
#include "slab.h"
#include "log.h"

void init_vga() {
    size_t framebuffers = get_framebuffer_count();
    intptr_t e = 0;
    if((e = vga_init()) < 0) {
        kwarn("Failed to initialise VGA: %s",status_str(e));
        return;
    }
    static char pathbuf[128];
    for(size_t id = 0; id < framebuffers; ++id) {
        stbsp_snprintf(pathbuf, sizeof(pathbuf), "vga%zu",id);
        Device* device = (Device*)cache_alloc(kernel.device_cache);
        if(!device) continue;
        e = create_vga_device(id, device);
        if(e < 0) {
            kwarn("Failed to initialise VGA (id=%zu): %s",id,status_str(e));
            cache_dealloc(kernel.device_cache, device);
            continue;
        }
        if((e = vfs_register_device(pathbuf, device)) < 0) {
            kwarn("Failed to register VGA device (id=%zu): %s",id,status_str(e));
            destroy_vga_device(device);
            cache_dealloc(kernel.device_cache, device);
            continue;
        }
        ktrace("Initialised VGA (id=%zu)",id);
    }
}
