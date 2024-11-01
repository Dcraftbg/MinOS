#include "fb.h"
#include "../../log.h"
#include <minos/fb/fb.h>
#include "../../kernel.h"
#include "../../mem/memregion.h"
// GLOBALS
Cache* fbd_cache=NULL;
static FsOps fbOps = {0};
static InodeOps inodeOps = {0};
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
    stats->pitch_bytes = device->fb.pitch_bytes;
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

// NOTE: Assumes the framebuffer is one continious chunk of physical memory (which it is)
// But in case this is ever an issue I'm putting this note here
static intptr_t fb_mmap(VfsFile* file, MmapContext* context, void** addr, size_t size_pages) {
    if(!file || !file->private || !context || !addr) return -INVALID_PARAM;
    intptr_t e;
    FbDevice* device = (FbDevice*)file->private;
    size_t pages = PAGE_ALIGN_UP(device->fb.height*device->fb.pitch_bytes)/PAGE_SIZE;
    if(size_pages > 0 && size_pages != pages) return -SIZE_MISMATCH;
    uint16_t rflags = 0;
    pageflags_t pflags = KERNEL_PFLAG_USER | KERNEL_PTYPE_USER | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE;
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
    MemoryList* insert_into = memlist_find_available(context->memlist, region, pages, pages);
    if(!insert_into) {
        e=-NOT_ENOUGH_MEM;
        goto err_no_insert_point;
    }
    *addr = (void*)region->address;
    if(!page_mmap(context->page_table, virt_to_phys(kernel.pml4, (uintptr_t)device->fb.addr), region->address, pages, pflags)) {
        e=-NOT_ENOUGH_MEM;
        goto mmap_fail;
    }
    return pages;
mmap_fail:
    page_unmap(context->page_table, virt_to_phys(kernel.pml4, (uintptr_t)device->fb.addr), pages);
err_no_insert_point:
    memlist_dealloc(list, NULL);
    return e;
}

static intptr_t fb_open(struct Inode* this, VfsFile* file, fmode_t mode) {
    file->private = this->private;
    file->ops = &fbOps;
    return 0;
}
static void fb_close(VfsFile* file) {
    file->private = NULL;
}

static intptr_t fb_init() {
    memset(&fbOps, 0, sizeof(fbOps));
    inodeOps.open = fb_open;
    fbOps.close = fb_close;
    fbOps.ioctl = fb_ioctl;
    fbOps.mmap  = fb_mmap;
    fbOps.close = fb_close;
    fbd_cache = create_new_cache(sizeof(FbDevice), "FbDevice");
    if(!fbd_cache) return -NOT_ENOUGH_MEM;
    return 0;
}
static intptr_t init_inode(Device* device, Inode* inode) {
    inode->private = device->private;
    inode->ops = &inodeOps;
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
        device->ops = &inodeOps;
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
