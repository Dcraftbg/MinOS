#pragma once
#include <stddef.h>
#include <stdint.h>
#include "framebuffer.h"
#include "utils.h"
#include <minos/status.h>
typedef struct {
    Framebuffer fb;
    size_t x, y;
} FbTextWriter;

intptr_t fb_draw_codepoint_at(Framebuffer* fm, size_t x, size_t y, int codepoint, uint32_t fg, uint32_t bg);
intptr_t fbwriter_draw_codepoint(FbTextWriter* fbt, int codepoint, uint32_t fg, uint32_t bg);
static void fbwriter_draw_cstr(FbTextWriter* fbt, const char* msg, uint32_t fg, uint32_t bg) {
    while(*msg) fbwriter_draw_codepoint(fbt, *msg++, fg, bg);
}
static void fbwriter_draw_cstr_default(FbTextWriter* fbt, const char* msg) {
    fbwriter_draw_cstr(fbt, msg, 0xdedede, 0);
}
