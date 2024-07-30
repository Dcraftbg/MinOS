#include <limine.h>
#include "mmap.h"
#include "string.h"
#include "kernel.h"
volatile struct limine_memmap_request limine_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

static void memmap_free_range(Memmap* map, void* from, size_t pages_count) {
    size_t page = PAGE_ALIGN_DOWN((size_t)from)/4096;
    assert(page+pages_count < map->page_count && "Out of range");
    for(size_t i = page; i < page+pages_count; ++i) {
        map->addr[i/8] &= ~(1 << (i%8));
    }
    map->page_available+=pages_count;
}

static void memmap_occupy_range(Memmap* map, void* from, size_t pages_count) {
    size_t page = PAGE_ALIGN_DOWN((size_t)from)/PAGE_SIZE;
    assert(page+pages_count < map->page_count && "Out of range");

    for(size_t i = page; i < page+pages_count; ++i) {
        map->addr[i/8] |= (1 << (i%8));
    }
    map->page_available-=pages_count;
}
void* memmap_alloc(Memmap* map, size_t pages_count) {
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
                memmap_occupy_range(map, (void*)(p*PAGE_SIZE), pages_count);
                return (void*)(p*PAGE_SIZE);
            }
            at++;
        }
    }
    return NULL;
}
void memmap_dealloc(Memmap* map, void* at, size_t pages_count) {
    memmap_free_range(map,at,pages_count);
}
#ifndef KERNEL_NO_MMAP_PRINT
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
#endif
// TODO: Refactor this. Uses a lot of limine specific things
void init_memmap() {
    assert(limine_memmap_request.response && "No memmap response???");
    size_t last_available = (size_t)-1;
    size_t biggest_size = 0;
    size_t biggest_avail= (size_t)-1;
    for(size_t i = 0; i < limine_memmap_request.response->entry_count; ++i) {
        struct limine_memmap_entry* entry = limine_memmap_request.response->entries[i];
#ifndef KERNEL_NO_MMAP_PRINT
        printf("%2zu> %p ",i,(void*)entry->base);
        if(entry->type < ARRAY_LEN(limine_memmap_str)) {
            printf("%-22s",limine_memmap_str[entry->type]);
        } else {
            printf("Unknown(%lu)",(uint64_t)entry->type);
        }
        printf(" %zu pages\n", entry->length / PAGE_SIZE);
#endif
        if(entry->type == LIMINE_MEMMAP_USABLE) {
             if(entry->length > biggest_size) {
                 biggest_avail = i;
                 biggest_size = entry->length;
             }
             last_available = i;
        }
    } 
    assert(last_available != -1 && "Cannot operate on 0 RAM");
    assert(biggest_avail != -1 && "Cannot operate on 0 RAM");
    struct limine_memmap_entry* biggest = limine_memmap_request.response->entries[biggest_avail];
    struct limine_memmap_entry* last = limine_memmap_request.response->entries[last_available];
    kernel.map.addr = (uint8_t*)biggest->base;
    kernel.map.page_count = PAGE_ALIGN_UP(last->base + last->length) / PAGE_SIZE;
    if((kernel.map.page_count+7)/8 > biggest->length) {
        printf("Could not fit memory map all in one place. Probably due to fragmentation\n");
        printf("The biggest chunk we found was %zu bytes and the whole memory map requires %zu bytes\n",biggest->length,kernel.map.page_count*PAGE_SIZE);
        kabort();
    }
    memset(kernel.map.addr, 0xFF, (kernel.map.page_count+7)/8);
    for(size_t i = 0; i < last_available; ++i) {
        struct limine_memmap_entry* entry = limine_memmap_request.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            memmap_free_range(&kernel.map, (void*)entry->base, entry->length/PAGE_SIZE);
        }
    }
    memmap_occupy_range(&kernel.map, (void*)kernel.map.addr, PAGE_ALIGN_UP((kernel.map.page_count+7)/8)/PAGE_SIZE);
}
