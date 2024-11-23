#include "fbwriter.h"
#include "fonts/zap-light16.h"
intptr_t fb_draw_codepoint_at(Framebuffer* fm, size_t x, size_t y, int codepoint, uint32_t fg, uint32_t bg) {
    if(codepoint > 127) return -UNSUPPORTED; // Unsupported
    if(fm->bpp != 32) return -UNSUPPORTED; // Because of optimisations we don't support anything besides 32 bits per pixel
    uint8_t* fontPtr = fontGlythBuffer + (codepoint*fontHeader.charsize);
    debug_assert(fontPtr + 16 < fontGlythBuffer+ARRAY_LEN(fontGlythBuffer));
    uint32_t* strip = (uint32_t*)(((uint8_t*)fm->addr) + fm->pitch_bytes*y);
    for(size_t cy = y; cy < y+16 && cy < fm->height; ++cy) {
        for(size_t cx = x; cx < x+8 && cx < fm->width; ++cx) {
            strip[cx] = ((*fontPtr & (0b10000000 >> (cx-x))) > 0) ? fg : bg;
        }
        fontPtr++;
        strip = (uint32_t*)((uint8_t*)strip + fm->pitch_bytes);
    }
    return 0;
}
void fbwriter_draw_sized_str(FbTextWriter* fbt, const char* msg, size_t len, uint32_t fg, uint32_t bg) {
    for(size_t i = 0; i < len; ++i) {
        fbwriter_draw_codepoint(fbt, msg[i], fg, bg);
    }
}
intptr_t fbwriter_draw_codepoint(FbTextWriter* fbt, int codepoint, uint32_t fg, uint32_t bg) {
    if((fbt->y+16) > fbt->fb.height) {
        size_t to_scroll = (fbt->y+16) - fbt->fb.height;
        fmbuf_scroll_up(&fbt->fb, to_scroll, bg);
        fbt->y -= to_scroll;
    }
    if(fbt->y >= fbt->fb.height) return 0;
    switch(codepoint) {
       case CODE_BLOCK:
         fmbuf_draw_rect(&fbt->fb, fbt->x, fbt->y, fbt->x+8, fbt->y+16, fg);
         if(fbt->x+8 >= fbt->fb.width) {
            fbt->y += 16;
            fbt->x = 0;
         }
         fbt->x += 8;
         break;
       case '\n':
         fbt->x=0;
         fbt->y+=16;
         break;
       case '\t': {
         size_t w = 8 * 4;
         fmbuf_draw_rect(&fbt->fb, fbt->x, fbt->y, fbt->x+w, fbt->y+16, bg);
         if(fbt->x+w >= fbt->fb.width) {
            fbt->y += 16;
            fbt->x = 0;
         }
         fbt->x += w;
       } break;
       case '\b': {
         if(fbt->x >= 8) {
             fbt->x -= 8;
             fmbuf_draw_rect(&fbt->fb, fbt->x, fbt->y, fbt->x+8, fbt->y+16, bg);
         }
       } break;
       case '\r':
         fbt->x = 0;
         break;
       case ' ':
         fmbuf_draw_rect(&fbt->fb, fbt->x, fbt->y, fbt->x+8, fbt->y+16, bg);
         if (fbt->x+8 >= fbt->fb.width) {
             fbt->y += 16;
             fbt->x = 0;
         }
         fbt->x += 8;
         break;
       default: {
         if(fbt->x+8 >= fbt->fb.width) {
             fbt->y += 16;
             fbt->x = 0;
         }
         intptr_t e = fb_draw_codepoint_at(&fbt->fb, fbt->x, fbt->y, codepoint, fg, bg);
         if(e >= 0) fbt->x += 8;
         return e;
       }
    }
    return 0;
}
