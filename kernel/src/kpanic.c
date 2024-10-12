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
    va_list va;
    va_start(va, fmt);
    kfatal_va(fmt, va);
    va_end(va);

    Framebuffer fb = get_framebuffer_by_id(0);
    if(fb.addr) {
        FbTextWriter writer = {
            .fb = fb,
            .x = 0,
            .y = 0,
        };
        fbwriter_draw_cstr(&writer, "Kernel panic!\n", 0xFFFFFF, 0xFF0000);
        va_list va;
        va_start(va, fmt);
        print_base(&writer, kpanic_fbwriter_write_sink, fmt, va);
        va_end(va);
    }
}
