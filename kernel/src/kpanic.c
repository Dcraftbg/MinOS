#include "kpanic.h"
#include "bootutils.h"
#include "framebuffer.h"
#include "fbwriter.h"
#include "serial.h"
#include "log.h"
#include <stdarg.h>
#include "print_base.h"

void kpanic_fbwriter_write_sink(void* fbt_void, const char* str, size_t len) {
    FbTextWriter* fbt = (FbTextWriter*)fbt_void;
    fbwriter_draw_sized_str(fbt, str, len, 0xFFFFFF, 0xFF0000);
}
void kpanic(const char* fmt, ...) {
    va_list va_fatal;
    va_list va_display;
    va_start(va_fatal, fmt);
    va_copy(va_display, va_fatal);
    kfatal_va(fmt, va_fatal);
    va_end(va_fatal);
    Framebuffer fb = get_framebuffer_by_id(0);
    if(fb.addr) {
        FbTextWriter writer = {
            .fb = fb,
            .x = 0,
            .y = 0,
        };
        fbwriter_draw_cstr(&writer, "Kernel panic!\n", 0xFFFFFF, 0xFF0000);
        print_base(&writer, kpanic_fbwriter_write_sink, fmt, va_display);
    }
    va_end(va_display);
    kabort();
}
