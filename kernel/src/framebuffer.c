#include "framebuffer.h"
#include "string.h"
#define swap_sz(a, b) \
    do { \
       size_t __swap = a; \
       a = b; \
       b = __swap; \
    } while(0)

void fmbuf_set_at(Framebuffer* this, size_t x, size_t y, uint32_t color) {
    debug_assert(this->bpp == 32); // We don't support grayscale yet :(  
    debug_assert(x < this->width);
    debug_assert(y < this->height);

    uint32_t* at = (uint32_t*)(((uint8_t*)this->addr) + this->pitch_bytes * y);
    at[x] = color;
}

void fmbuf_draw_rect(Framebuffer* this, size_t left, size_t top, size_t right, size_t bottom, uint32_t color) {
    debug_assert(this->bpp == 32); // We don't support grayscale yet :(  
    if(left > right) swap_sz(left, right);
    if(top > bottom) swap_sz(top, bottom);
    debug_assert(left < this->width && right <= this->width);
    debug_assert(top < this->height && bottom <= this->height);

    uint32_t* at = (uint32_t*)this->addr;
    at = (uint32_t*)(((uint8_t*)at) + this->pitch_bytes * top);
    for(size_t y = top; y < bottom; ++y) {
        for(size_t x = left; x < right; ++x) {
            at[x] = color;
        } 
        at = (uint32_t*)(((uint8_t*)at) + this->pitch_bytes);
    }
}

void fmbuf_scroll_up(Framebuffer* this, size_t rows, uint32_t bg) {
    debug_assert(this->height >= rows);
    uint8_t* addr=this->addr;
    size_t bytes =this->bpp/8;
    for(size_t y = 0; y < this->height-rows; ++y) {
        memmove(addr, addr+rows*this->pitch_bytes, this->width*bytes);
        addr+=this->pitch_bytes;
    }
    fmbuf_draw_rect(this, 0, this->height-rows, this->width, this->height, bg);
}
