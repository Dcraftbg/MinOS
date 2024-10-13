#include "prog_bar.h"

#ifdef PROGRESS_BAR
void update_bar(size_t at, const char* msg) {
    Framebuffer fb = get_framebuffer_by_id(0);
    if(fb.addr) {
        size_t height = 10;
        size_t width = (fb.width-30) * at / TOTAL_STEPS;
        size_t x = 15;
        size_t y = (fb.height-height-5); // (fb.height-height) / 2;
        fmbuf_draw_rect(&fb, x, y, x+width, y+height, 0x00FF00);
        size_t texoff = 20;
        fmbuf_draw_rect(&fb, x, y-texoff, x+fb.width-15, y, 0);
        FbTextWriter writer = {
            .fb = fb,
            .x = x,
            .y = y-texoff,
        };
        fbwriter_draw_cstr_default(&writer, msg);
    }
}
size_t step=0;
#endif
