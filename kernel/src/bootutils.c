#include "bootutils.h"
#include "page.h"
#include "assert.h"
#include "kernel.h"
#include "utils.h"
#include "log.h"
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
static volatile struct limine_module_request limine_module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
};

static volatile struct limine_rsdp_request limine_rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
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

static const char* limine_memmap_str[] = {
    "Usable",
    "Reserved",
    "Acpi Reclaimable",
    "Acpi NVS",
    "Bad Memory",
    "Bootloader Reclaimable",
    "Kernel and Modules",
    "Framebuffer"
};
void boot_map_phys_memory() { 
    for(size_t i = 0; i < limine_memmap_request.response->entry_count; ++i) {
        struct limine_memmap_entry* entry = limine_memmap_request.response->entries[i];
        pageflags_t flags = KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_EXEC_DISABLE;
        size_t pages = entry->length/PAGE_SIZE;
        if(
            entry->type != LIMINE_MEMMAP_FRAMEBUFFER &&
            entry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES
        ) {
            if(entry->base >= PHYS_RAM_MIRROR_SIZE) {
                ktrace("OutOfBou %-22s (%p -> %p) %zu pages", limine_memmap_str[entry->type], (void*)PAGE_ALIGN_DOWN(entry->base), (void*)PAGE_ALIGN_DOWN(entry->base | KERNEL_MEMORY_MASK), pages);
                continue;
            }
            if(entry->base+(pages*PAGE_SIZE) > PHYS_RAM_MIRROR_SIZE)  {
                pages = (PHYS_RAM_MIRROR_SIZE-entry->base)/PAGE_SIZE;
            }
        }
        switch(entry->type) {
        case LIMINE_MEMMAP_FRAMEBUFFER:
            flags |= KERNEL_PFLAG_WRITE_COMBINE;
        case LIMINE_MEMMAP_USABLE:
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            if(!
               page_mmap(
                 kernel.pml4,
                 PAGE_ALIGN_DOWN(entry->base),
                 PAGE_ALIGN_DOWN(entry->base | KERNEL_MEMORY_MASK),
                 pages,
                 flags
               )
            ) {
                kpanic("Failed to map region %zu of type %s. (%p -> %p) %zu pages. Available memory in bitmap: %zu pages", i, limine_memmap_str[entry->type], (void*)PAGE_ALIGN_DOWN(entry->base), (void*)PAGE_ALIGN_DOWN(entry->base | KERNEL_MEMORY_MASK), pages, kernel.map.page_available);
            }
            kinfo("Mapping   %-22s (%p -> %p) %zu pages", limine_memmap_str[entry->type], (void*)PAGE_ALIGN_DOWN(entry->base), (void*)PAGE_ALIGN_DOWN(entry->base | KERNEL_MEMORY_MASK), (size_t)pages);
            break;
        default:
            ktrace("Skipping %-22s (%p -> %p) %zu pages", limine_memmap_str[entry->type], (void*)PAGE_ALIGN_DOWN(entry->base), (void*)PAGE_ALIGN_DOWN(entry->base | KERNEL_MEMORY_MASK), (size_t)pages);
        }
    }
#if 0
    // Fuck you limine
    kinfo("Framebuffer Patches");
    size_t framebuffer_count = get_framebuffer_count();
    for(size_t i = 0; i < framebuffer_count; ++i) {
        Framebuffer framebuffer = get_framebuffer_by_id(i);
        if(!framebuffer.addr) continue;
        if(((uintptr_t)framebuffer.addr-KERNEL_MEMORY_MASK) > PHYS_RAM_MIRROR_SIZE) {
            // NOTE: assumes addr is in HHDM
            paddr_t   phys = ((paddr_t)framebuffer.addr)-KERNEL_MEMORY_MASK;
            uintptr_t virt = (uintptr_t)framebuffer.addr;
            size_t   pages = PAGE_ALIGN_UP(framebuffer.height*framebuffer.pitch_bytes)/PAGE_SIZE;
            pageflags_t flags = KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_EXEC_DISABLE | KERNEL_PFLAG_WRITE_COMBINE;
            if(!
               page_mmap(
                 kernel.pml4,
                 phys,
                 virt,
                 pages,
                 flags
               )
            ) {
                kpanic("Failed to map Framebuffer#%zu. (%p -> %p) %zu pages. Available memory in bitmap: %zu pages", i, (void*)phys, (void*)virt, pages, kernel.map.page_available);
            }
            kinfo("Framebuffer#%zu (%p -> %p) %zu pages", i, (void*)phys, (void*)virt, (size_t)pages);
        }
    }
#endif
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
static bool fill_module_from_limine(BootModule* module, struct limine_file* limine) {
    if(!limine) return false;
    module->path    = limine->path;
    module->cmdline = limine->cmdline;
    module->data    = limine->address;
    module->size    = limine->size;
    return true;
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

size_t get_bootmodules_count() {
    if(!limine_module_request.response) return 0;
    return limine_module_request.response->module_count;
}
bool get_bootmodule(size_t i, BootModule* module) {
    if(!limine_module_request.response) return false;
    if(i >= limine_module_request.response->module_count) return false;
    return fill_module_from_limine(module, limine_module_request.response->modules[i]);
}

bool find_bootmodule(const char* path, BootModule* module) {
    size_t modules = get_bootmodules_count();
    for(size_t i = 0; i < modules; ++i) {
        if(get_bootmodule(i, module)) {
            if(strcmp(module->path, path)==0) return true;
        }
    }
    return false;
}

char* get_kernel_cmdline() {
    if(!limine_kernel_file_request.response || !limine_kernel_file_request.response->kernel_file) return NULL;
    return limine_kernel_file_request.response->kernel_file->cmdline;
}

paddr_t get_rsdp_addr() {
    if(!limine_rsdp_request.response) return 0;
    return ((uintptr_t)limine_rsdp_request.response->address)-KERNEL_MEMORY_MASK;
}
