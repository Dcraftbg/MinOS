#include "fb.h"
#include "../../log.h"
#include <minos/fb/fb.h>
#include "../../kernel.h"
#include "../../mem/memregion.h"
// GLOBALS
static Cache* fbd_cache=NULL;
// TODO: Some sort of synchronization for Framebuffers like with inodes. Maybe TtyFb should borrow Framebuffer or something
typedef struct {
    Framebuffer fb;
} FbDevice;
// void destroy_fb_device(Device* device) {
//     if(!device || !device->priv) return;
//     FbDevice* fbd = device->priv;
//     cache_dealloc(fbd_cache, fbd);
// }
static intptr_t fb_get_stats(FbDevice* device, FbStats* stats) {
    stats->width = device->fb.width;
    stats->height = device->fb.height;
    stats->bpp = device->fb.bpp;
    stats->pitch_bytes = device->fb.pitch_bytes;
    return 0;
}
// TODO: Maybe do an ioctl function map. IDRK
static intptr_t fb_ioctl(Inode* file, Iop op, void* arg) {
    if(!file || !file->priv) return -INVALID_PARAM;
    FbDevice* device = (FbDevice*)file->priv;
    switch(op) {
    case FB_IOCTL_GET_STATS: return fb_get_stats(device, (FbStats*)arg);
    }
    return -INVALID_PARAM;
}
#include "task.h"
// NOTE: Assumes the framebuffer is one continious chunk of physical memory (which it is)
// But in case this is ever an issue I'm putting this note here
static intptr_t fb_mmap(Inode* file, MmapContext* context, void** addr, size_t size_pages) {
    if(!file || !file->priv || !context || !addr) return -INVALID_PARAM;
    intptr_t e;
    FbDevice* device = (FbDevice*)file->priv;
    size_t pages = PAGE_ALIGN_UP(device->fb.height*device->fb.pitch_bytes)/PAGE_SIZE;
    paddr_t phys = virt_to_phys(kernel.pml4, (uintptr_t)device->fb.addr);
    if(size_pages > 0 && size_pages != pages) return -SIZE_MISMATCH;
    uint16_t rflags = 0;
    pageflags_t pflags = 
        KERNEL_PFLAG_USER |
        KERNEL_PTYPE_USER |
        KERNEL_PFLAG_PRESENT |
        KERNEL_PFLAG_EXEC_DISABLE |
        KERNEL_PFLAG_WRITE |
        KERNEL_PFLAG_WRITE_COMBINE;

    if(file->mode & MODE_WRITE) {
        pflags |= KERNEL_PFLAG_WRITE;
        rflags |= MEMREG_WRITE;
    }

    MemoryRegion* region = memregion_new(
        rflags,
        pflags,
        0, 0
    ); 
    if(!region) return -NOT_ENOUGH_MEM;
    MemoryList* list = memlist_new(region);
    if(!list) {
        memregion_drop(region, NULL);
        return -NOT_ENOUGH_MEM;
    }
    // FIXME: Consider taking into consideration *addr
    MemoryList* insert_into = memlist_find_available(context->memlist, region, (void*)current_task()->image.eoe, pages, pages);
    if(!insert_into) {
        e=-NOT_ENOUGH_MEM;
        goto err_no_insert_point;
    }
    *addr = (void*)region->address;
    if(!page_mmap(context->page_table, phys, region->address, pages, pflags)) {
        e=-NOT_ENOUGH_MEM;
        goto mmap_fail;
    }
    list_append(&list->list, &insert_into->list);
    return pages*PAGE_SIZE;
mmap_fail:
    page_unmap(context->page_table, phys, pages);
err_no_insert_point:
    memlist_dealloc(list, NULL);
    return e;
}
static void fb_cleanup(Inode* inode) {
    cache_dealloc(fbd_cache, (FbDevice*)inode->priv);
}
static InodeOps inodeOps = {
    .ioctl = fb_ioctl,
    .mmap = fb_mmap,
};
static intptr_t fb_init() {
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
        Inode* device = new_inode();
        if(!device) return -NOT_ENOUGH_MEM;
        FbDevice* fbd = (FbDevice*)cache_alloc(fbd_cache);
        if(!fbd) {
            e = -NOT_ENOUGH_MEM;
            goto err_null_fb_dev;
        }
        fbd->fb = get_framebuffer_by_id(i);
        if(!fbd->fb.addr) goto err_invalid_framebuffer;
        device->priv = fbd;
        device->ops = &inodeOps;
        stbsp_snprintf(namebuf, sizeof(namebuf), "fb%zu", i);
        if((e=vfs_register_device(namebuf, device))) goto err_register_device;
        continue;
err_register_device:
err_invalid_framebuffer:
        cache_dealloc(fbd_cache, fbd);
err_null_fb_dev:
        idrop(device);
        if(e) return e;
    }
    return 0;
}
