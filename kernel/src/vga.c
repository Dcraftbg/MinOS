#include "vga.h"
#include "bootutils.h"
#include "kernel.h"
#include "slab.h"

void init_vga() {
    size_t framebuffers = get_framebuffer_count();
    intptr_t e = 0;
    if((e = vga_init()) < 0) {
        printf("WARN: Failed to initialise VGA: %s\n",status_str(e));
        return;
    }
    static char pathbuf[128];
    for(size_t id = 0; id < framebuffers; ++id) {
        stbsp_snprintf(pathbuf, sizeof(pathbuf), "vga%zu",id);
        Device* device = (Device*)cache_alloc(kernel.device_cache);
        if(!device) continue;
        e = create_vga_device(id, device);
        if(e < 0) {
            printf("WARN: Failed to initialise VGA (id=%zu): %s\n",id,status_str(e));
            cache_dealloc(kernel.device_cache, device);
            continue;
        }
        if((e = vfs_register_device(pathbuf, device)) < 0) {
            printf("WARN: Failed to register VGA device (id=%zu): %s\n",id,status_str(e));
            destroy_vga_device(device);
            cache_dealloc(kernel.device_cache, device);
            continue;
        }
        printf("TRACE: Initialised VGA (id=%zu)\n",id);
    }
}
