#include "bootutils.h"
#include "page.h"
#include "assert.h"
#include "kernel.h"
#include "utils.h"
#include <limine.h>
extern volatile struct limine_memmap_request limine_memmap_request;

static volatile struct limine_framebuffer_request limine_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};
static volatile struct limine_kernel_address_request limine_kernel_address_request = {
     .id = LIMINE_KERNEL_ADDRESS_REQUEST,
     .revision = 0, 
};
static volatile struct limine_kernel_file_request limine_kernel_file_request = {
     .id = LIMINE_KERNEL_FILE_REQUEST,
     .revision = 0,
};
void kernel_file_data(void** data, size_t* size) {
     assert(limine_kernel_file_request.response && limine_kernel_file_request.response->kernel_file);
     *data = limine_kernel_file_request.response->kernel_file->address;
     *size = limine_kernel_file_request.response->kernel_file->size;
}
void kernel_mempair(Mempair* mempair) {
     assert(limine_kernel_address_request.response);
     mempair->virt = limine_kernel_address_request.response->virtual_base ;
     mempair->phys = limine_kernel_address_request.response->physical_base;
}
void boot_map_phys_memory() { 
    for(size_t i = 0; i < limine_memmap_request.response->entry_count; ++i) {
        struct limine_memmap_entry* entry = limine_memmap_request.response->entries[i];
        switch(entry->type) {
        case LIMINE_MEMMAP_USABLE:
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        case LIMINE_MEMMAP_FRAMEBUFFER:
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            assert(
               page_mmap(
                 kernel.pml4,
                 PAGE_ALIGN_DOWN(entry->base),
                 PAGE_ALIGN_DOWN(entry->base | KERNEL_MEMORY_MASK),
                 entry->length/PAGE_SIZE,
                 KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_EXEC_DISABLE
               )
            );
            break;
        default:
        }
    }
}
static Framebuffer fmbuf_from_limine(struct limine_framebuffer* buf) {
    static_assert(sizeof(Framebuffer) == 40, "Update fmbuf_from_limine");
    return (Framebuffer) {
        buf->address,
        buf->bpp,
        buf->width,
        buf->height,
        buf->pitch
    };
}
Framebuffer get_framebuffer_by_id(size_t id) {
    if(!limine_framebuffer_request.response) return (Framebuffer){0}; 
    if(limine_framebuffer_request.response->framebuffer_count <= id) return (Framebuffer){0}; 
    return fmbuf_from_limine(limine_framebuffer_request.response->framebuffers[id]);
}

size_t get_framebuffer_count() {
    if(!limine_framebuffer_request.response) return 0; 
    return limine_framebuffer_request.response->framebuffer_count;
}
