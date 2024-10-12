#pragma once
#include <stddef.h>
#include <stdint.h>
#include "framebuffer.h"
#include "utils.h"
#include "string.h"
#include <minos/status.h>
typedef struct {
    Framebuffer fb;
    size_t x, y;
} FbTextWriter;
#define FBWRITER_DEFAULT_FG 0xdedede
#define FBWRITER_DEFAULT_BG 0
intptr_t fb_draw_codepoint_at(Framebuffer* fm, size_t x, size_t y, int codepoint, uint32_t fg, uint32_t bg);
intptr_t fbwriter_draw_codepoint(FbTextWriter* fbt, int codepoint, uint32_t fg, uint32_t bg);
void fbwriter_draw_sized_str(FbTextWriter* fbt, const char* msg, size_t len, uint32_t fg, uint32_t bg);
static void fbwriter_draw_cstr(FbTextWriter* fbt, const char* msg, uint32_t fg, uint32_t bg) {
    fbwriter_draw_sized_str(fbt, msg, strlen(msg), fg, bg);
}
static void fbwriter_draw_cstr_default(FbTextWriter* fbt, const char* msg) {
    fbwriter_draw_cstr(fbt, msg, FBWRITER_DEFAULT_FG, FBWRITER_DEFAULT_BG);
}
