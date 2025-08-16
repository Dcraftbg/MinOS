#include <limine.h>
#include "bitmap.h"
#include "string.h"
#include "kernel.h"
#include "bootutils.h"
#include "log.h"
volatile struct limine_memmap_request limine_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};
volatile struct limine_hhdm_request limine_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};
static void bitmap_free_range(Bitmap* map, void* from, size_t pages_count) {
    size_t page = PAGE_ALIGN_DOWN((size_t)from)/PAGE_SIZE;
    assert(page+pages_count <= map->page_count && "Out of Range");
    for(size_t i = page; i < page+pages_count; ++i) {
        map->addr[i/8] &= ~(1 << (i%8));
    }
    map->page_available+=pages_count;
}

static void bitmap_occupy_range(Bitmap* map, void* from, size_t pages_count) {
    size_t page = PAGE_ALIGN_DOWN((size_t)from)/PAGE_SIZE;
    assert(page+pages_count <= map->page_count && "Out of range");

    for(size_t i = page; i < page+pages_count; ++i) {
        map->addr[i/8] |= (1 << (i%8));
    }
    map->page_available-=pages_count;
}
void* bitmap_alloc(Bitmap* map, size_t pages_count) {
    if(map->page_available < pages_count) return NULL;
    size_t block_count = (map->page_count+7)/8;
    for(size_t i=0; i < block_count; ++i) {
        if(map->addr[i] == 0xFF) continue;
        uint8_t b = map->addr[i];
        size_t p = i*8;
        while((b & 1) == 1) {
           b >>= 1;
           p++;
        }
        size_t at = p;
        while(((map->addr[at/8] & (1<<(at%8))) == 0) && at < map->page_count) {
            if((at-p) == pages_count) {
                bitmap_occupy_range(map, (void*)(p*PAGE_SIZE), pages_count);
                return (void*)(p*PAGE_SIZE);
            }
            at++;
        }
    }
    return NULL;
}
void bitmap_dealloc(Bitmap* map, void* at, size_t pages_count) {
    bitmap_free_range(map,at,pages_count);
}
// TODO: Refactor this. Uses a lot of limine specific things
void init_bitmap() {
    assert(limine_memmap_request.response && "No memmap response???");
    assert(limine_hhdm_request.response && "No hhdm respo???");
    assert(limine_hhdm_request.response->offset == KERNEL_MEMORY_MASK && "HHDM does not match");
    size_t last_available = (size_t)-1;
    size_t biggest_size = 0;
    size_t biggest_avail= (size_t)-1;
    for(size_t i = 0; i < limine_memmap_request.response->entry_count; ++i) {
        struct limine_memmap_entry* entry = limine_memmap_request.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && entry->base < PHYS_RAM_MIRROR_SIZE) {
             size_t pages = entry->length/PAGE_SIZE;
             if(entry->base+(pages*PAGE_SIZE) > PHYS_RAM_MIRROR_SIZE)  {
                 pages = (PHYS_RAM_MIRROR_SIZE-entry->base)/PAGE_SIZE;
             }
             if(entry->length > biggest_size) {
                 biggest_avail = i;
                 biggest_size = pages*PAGE_SIZE;
             }
             last_available = i;
        }
    } 
    assert(last_available != -1 && "Cannot operate on 0 RAM");
    assert(biggest_avail != -1 && "Cannot operate on 0 RAM");
    struct limine_memmap_entry* biggest = limine_memmap_request.response->entries[biggest_avail];
    struct limine_memmap_entry* last = limine_memmap_request.response->entries[last_available];
    assert(biggest->base < PHYS_RAM_MIRROR_SIZE && "Biggest data block has to be below 4GB. Sorry");
    kernel.map.addr = (uint8_t*)(biggest->base | KERNEL_MEMORY_MASK);
    kernel.map.page_count = (PAGE_ALIGN_UP(last->base) + PAGE_ALIGN_UP(last->length)) / PAGE_SIZE;
    if((kernel.map.page_count+7)/8 > biggest->length) {
        kpanic(
            "Could not fit memory map all in one place. Probably due to fragmentation\n"
            "The biggest chunk we found was %zu bytes and the whole memory map requires %zu bytes\n",
            biggest->length,
            (size_t)(kernel.map.page_count*PAGE_SIZE)
        );
    }
    memset(kernel.map.addr, 0xFF, (kernel.map.page_count+7)/8);
    for(size_t i = 0; i <= last_available; ++i) {
        struct limine_memmap_entry* entry = limine_memmap_request.response->entries[i];
        size_t pages = entry->length/PAGE_SIZE;
        if(entry->base+(pages*PAGE_SIZE) > PHYS_RAM_MIRROR_SIZE)  {
            pages = (PHYS_RAM_MIRROR_SIZE-entry->base)/PAGE_SIZE;
        }
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            bitmap_free_range(&kernel.map, (void*)entry->base, pages);
        }
    }
    bitmap_occupy_range(&kernel.map, (void*)biggest->base, PAGE_ALIGN_UP((kernel.map.page_count+7)/8)/PAGE_SIZE);
}
