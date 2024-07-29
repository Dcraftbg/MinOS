#include "utils.h"
#include <limine.h>
#include <stddef.h>
#include "assert.h"
extern volatile struct limine_framebuffer_request limine_framebuffer_request;
void kabort() {
#if 0
    assert(limine_framebuffer_request.response && "No framebuffer :(");
    assert(limine_framebuffer_request.response->framebuffer_count == 1 && limine_framebuffer_request.response->framebuffers[0] -> bpp == 32 && "Unsupported framebuffer format");
    struct limine_framebuffer* fb = limine_framebuffer_request.response->framebuffers[0];
    for(size_t y = 0; y < fb->height; ++y) {
        for(size_t x = 0; x < fb->width; ++x) {
            *((uint32_t*)((uint8_t*)fb->address + y*fb->pitch)+x) = 0xFFFF0000;//0xFF212121;
        }
    }
#endif
    for(;;) {
        asm volatile ("hlt");
    }
}
