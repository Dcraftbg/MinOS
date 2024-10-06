#include "kpanic.h"
#include "bootutils.h"
#include "framebuffer.h"
#include "fbwriter.h"
#include "serial.h"
void kpanic(const char* msg) {
    serial_printstr(msg);
    Framebuffer fb = get_framebuffer_by_id(0);
    if(fb.addr) {
        fmbuf_fill(&fb, 0xFF0000);
        FbTextWriter writer = {
            .fb = fb,
            .x = 0,
            .y = 0,
        };
        fbwriter_draw_cstr(&writer, "Kernel panic!\n", 0xFFFFFF, 0xFF0000);
        fbwriter_draw_cstr(&writer, msg, 0xFFFFFF, 0xFF0000);
    }
}
