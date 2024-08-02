#pragma once
#include <stdint.h>
#include <stddef.h>
#include "assert.h"
typedef struct {
    uint8_t* addr;
    uintptr_t bpp;
    size_t width;
    size_t height;
    size_t pitch_bytes;
} Framebuffer;
// TODO: Introduce:
// typedef struct {
//     uint8_t* data;
//     size_t width;
//     size_t height;
//     size_t pitch_bits;
// } Bitmap;
// void fmbuf_draw_bitmap(Framebuffer* this, size_t left, size_t top, Bitmap bitmap, uint32_t color);
// NOTE: assumes color is at MAX a 32 bit value.
// If a framebuffer has a bpp = 1 that would indicate that any color value > 0 will be considered a 1 and any value equal to 0 a 0
void fmbuf_draw_rect(Framebuffer* this, size_t left, size_t top, size_t right, size_t bottom, uint32_t color);
void fmbuf_set_at(Framebuffer* this, size_t x, size_t y, uint32_t color);
static void fmbuf_fill(Framebuffer* this, uint32_t color) {
    fmbuf_draw_rect(this, 0, 0, this->width, this->height, color);
}
