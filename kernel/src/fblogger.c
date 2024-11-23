#include "fblogger.h"
intptr_t fb_write_str(Logger* logger, const char* str, size_t len) {
    FbTextWriter* writer = (FbTextWriter*)logger->private;
    fbwriter_draw_sized_str(writer, str, len, FBWRITER_DEFAULT_FG, FBWRITER_DEFAULT_BG);
    return len;
}
FbTextWriter fbt0={0};
Logger fb_logger = {
    .log = NULL,
    .write_str = fb_write_str,
    .draw_color = NULL,
    .level = LOG_ALL,
    .private = &fbt0 
};
intptr_t init_fb_logger() {
    intptr_t e;
    fbt0.fb = get_framebuffer_by_id(0);
    if(!fbt0.fb.addr) return -NOT_FOUND;
    if((e=logger_init(&fb_logger)) < 0) return e;
    return 0;
}
