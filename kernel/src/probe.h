#pragma once
static uint32_t wang_seed;
static inline uint32_t wang_hash(uint32_t seed) {
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
} 
static inline uint32_t wang_rand() {
    return wang_seed = wang_hash(wang_seed);
}
#include <limine.h>
static volatile struct limine_boot_time_request limine_boot_time_request = {
    .id = LIMINE_BOOT_TIME_REQUEST,
    .revision = 0,
};
static void probe_bitmap() {
#define PROBES 1000
    typedef struct {
        void* ptr;
        size_t size;
    } ProbeBM;
    static ProbeBM probes[PROBES] = {0};
    size_t collisions = 0;
    for(size_t i = 0; i < PROBES; ++i) {
        ProbeBM* probe = &probes[i];
        probe->size = (wang_rand() % (100-1)) + 1;
        probe->ptr = kernel_malloc(probe->size * PAGE_SIZE);
        printf("%zu> Probe %zu = %p\n", i, probe->size, probe->ptr);
        if(probe->ptr) {
            memset(probe->ptr, 0, probe->size * PAGE_SIZE);
        }
    }
    for(size_t i = 0; i < PROBES; ++i) {
        ProbeBM* p1 = &probes[i];
        if(p1->ptr == NULL) {
            printf("%zu> Skipping of size %zu\n", i, p1->size);
            continue;
        }
        void* end_p1 = p1->ptr + ((p1->size-1)*PAGE_SIZE);
        for(size_t j = 0; j < PROBES; ++j) {
            if(i == j) continue;
            ProbeBM* p2 = &probes[j];
            if(p2->ptr == NULL) continue;
            void* end_p2 = p2->ptr + ((p2->size-1)*PAGE_SIZE);
            if(p1->ptr < end_p2 && p2->ptr < end_p1) {  
                collisions++;
                printf("COLLISION! %p of size %zu and %p of size %zu\n", p1->ptr, p1->size, p2->ptr, p2->size);
                void* base = p1->ptr < p2->ptr ? p1->ptr : p2->ptr;
                for(size_t i = 0; i < (p1->ptr - base); ++i) {
                    serial_print_u8(' ');
                }
                serial_print_u8('[');
                for(size_t i = 0; i < p1->size; ++i) {
                    serial_print_u8('1');
                }
                serial_print_u8(']');
                serial_print_u8('\n');

                for(size_t i = 0; i < (p2->ptr - base); ++i) {
                    serial_print_u8(' ');
                }
                serial_print_u8('[');
                for(size_t i = 0; i < p2->size; ++i) {
                    serial_print_u8('2');
                }
                serial_print_u8(']');
                serial_print_u8('\n');
            }
        }
    }
    printf("Collisions: %zu\n", collisions);
    printf("Available pages: %zu\n", kernel.map.page_available);
#undef PROBES
}
static void probe_slab() {
    typedef struct {
        void* ptr;
    } ProbeSlab;
    size_t fancy_size = 
        80;
    // ((wang_rand() % (100-16)) + 16) & (~0xf);
    printf("Creating fancy cache for size=%zu\n", fancy_size);
    struct Cache* fancy_cache = create_new_cache(fancy_size, "Fancy");
    assert(fancy_cache);

#define PROBES 1000
    static ProbeSlab probes[PROBES] = {0};
    size_t collisions = 0;
    for(size_t i = 0; i < PROBES; ++i) {
        ProbeSlab* probe = &probes[i];
        probe->ptr = cache_alloc(fancy_cache);
        printf("%zu> Probe %p\n", i, probe->ptr);
        if(probe->ptr) {
            memset(probe->ptr, 0, fancy_size);
        }
    }
    for(size_t i = 0; i < PROBES; ++i) {
        ProbeSlab* p1 = &probes[i];
        if(p1->ptr == NULL) {
            printf("%zu> Skipping\n", i);
            continue;
        }
        for(size_t j = 0; j < PROBES; ++j) {
            if(i == j) continue;
            ProbeSlab* p2 = &probes[j];
            if(p1->ptr == p2->ptr) {
                printf("COLLISION! Between %zu and %zu\n", i, j);
                collisions++;
            }
        }
    }
    printf("Collisions: %zu\n", collisions);
    printf("Available pages: %zu\n", kernel.map.page_available);
#undef PROBES
}
