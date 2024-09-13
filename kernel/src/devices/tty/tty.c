#include "tty.h"
#include "../../log.h"
#include "../../debug.h"
#include "../../kernel.h"
#include "../../fonts/zap-light16.h"
#include <minos/keycodes.h>
#include <minos/key.h>

static FsOps ttyOps = {0};
#define KEY_BYTES ((MINOS_KEY_COUNT+7)/8)
typedef struct {
   // Is key down?
   uint8_t state[KEY_BYTES];
} Keyboard;
static void key_set(Keyboard* kb, uint16_t code, uint8_t released) {
   uint8_t down = released == 0;
   //debug_assert(code < ARRAY_LEN(kb->state));
   kb->state[code/8] &= ~(1 << (code % 8));
   kb->state[code/8] |= down << (code%8);
}
static bool key_get(Keyboard* kb, uint16_t code) {
    //debug_assert(code < ARRAY_LEN(kb->state));
    return kb->state[code/8] & (1<<(code%8));
}

static uint8_t US_QWERTY_SHIFTED[256] = {
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
   0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x22,
   0x28, 0x29, 0x2A, 0x2B, 0x3C, 0x5F, 0x3E, 0x3F,
   0x29, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26,
   0x2A, 0x28, 0x3A, 0x3A, 0x3C, 0x2B, 0x3E, 0x3F,
   0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
   0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
   0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
   0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x5E, 0x5F,
   0x7E, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
   0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
   0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
   0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
   0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
   0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
   0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
   0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
   0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
   0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
   0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
   0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
   0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
   0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
   0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
   0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
   0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
   0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
   0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
   0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
static int key_unicode(Keyboard* keyboard, uint16_t code) {
    switch(code) {
    case MINOS_KEY_ENTER:
        if(key_get(keyboard, MINOS_KEY_ENTER)) return '\n';
        return 0;
    default:
        if(code >= 256 || !key_get(keyboard, code)) return 0;
        if(key_get(keyboard, MINOS_KEY_LEFT_SHIFT) || key_get(keyboard, MINOS_KEY_RIGHT_SHIFT)) {
            return US_QWERTY_SHIFTED[code];
        }
        if(code >= 'A' && code <= 'Z') return code-'A' + 'a';
        return code;
    }
}
enum {
     TTY_INPUT_KEYBOARD,
     TTY_INPUT_COUNT,
};

enum {
     TTY_OUTPUT_DISPLAY,
     TTY_OUTPUT_COUNT
};
typedef struct {
     Framebuffer fb;
     size_t x, y;
     bool blink;
     size_t blink_time;
} TtyFb;
typedef struct {
     uint8_t input_kind;
     union {
         struct { 
             VfsFile keyboard;
             Keyboard keystate;
         } as_kb;
     } input;

     uint8_t output_kind;
     union {
         TtyFb as_framebuffer;
     } output;
} TtyDevice;
intptr_t tty_draw_codepoint_at(Framebuffer* fm, size_t x, size_t y, int codepoint, uint32_t fg, uint32_t bg) {
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

static intptr_t tty_draw_codepoint(TtyFb* fb, int codepoint, uint32_t fg, uint32_t bg) {
    if(fb->y >= fb->fb.height) return 0;
    switch(codepoint) {
       case '\n':
         fb->x=0;
         fb->y+=16;
         break;
       case '\t': {
         size_t w = 8 * 4;
         fmbuf_draw_rect(&fb->fb, fb->x, fb->y, fb->x+w, fb->y+16, bg);
         if(fb->x+w >= fb->fb.width) {
            fb->y += 16;
            fb->x = 0;
         }
         fb->x += w;
       } break;
       case '\r':
         fb->x = 0;
         break;
       case ' ':
         fmbuf_draw_rect(&fb->fb, fb->x, fb->y, fb->x+8, fb->y+16, bg);
         if (fb->x+8 >= fb->fb.width) {
             fb->y += 16;
             fb->x = 0;
         }
         fb->x += 8;
         break;
       default: {
         if(fb->x+8 >= fb->fb.width) {
             fb->y += 16;
             fb->x = 0;
         }
         intptr_t e = tty_draw_codepoint_at(&fb->fb, fb->x, fb->y, codepoint, fg, bg);
         if(e >= 0) fb->x += 8;
         return e;
       }
    }
    return 0;
}
static Cache* tty_cache = NULL;
static intptr_t tty_open(struct Device* this, VfsFile* file, fmode_t mode) {
     file->private = this->private;
     return 0;
}

static void tty_close(VfsFile* file) {
    file->private = NULL;
}

#define TTY_MILISECOND_BLINK 500
#define TTY_BLINK_WIDTH 8
#define TTY_BLINK_HEIGHT 16
static void ttyfb_fill_blink(TtyFb* fb, uint32_t color) {
     fmbuf_draw_rect(&fb->fb, fb->x, fb->y, fb->x+TTY_BLINK_WIDTH, fb->y+TTY_BLINK_HEIGHT, color);
}

static const uint32_t blink_color[2] = {
     VGA_BG,
     VGA_FG
};
static intptr_t tty_dev_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
     (void)offset;
     TtyDevice* tty = (TtyDevice*)file->private;
     assert(tty->output_kind == TTY_OUTPUT_DISPLAY);
     TtyFb* fb = &tty->output.as_framebuffer;
     if(fb->blink) ttyfb_fill_blink(fb, VGA_BG);
     for(size_t i = 0; i < size; ++i) {
        tty_draw_codepoint(fb, ((uint8_t*)buf)[i], VGA_FG, VGA_BG);
     }
     return 0;
}

// TODO: Unicode support
static intptr_t tty_dev_read(VfsFile* file, void* buf, size_t size, off_t offset) {
     (void)offset;
     TtyDevice* tty = (TtyDevice*)file->private;
     intptr_t e;
     Key key;
     size_t left=size;

     assert(tty->input_kind == TTY_OUTPUT_DISPLAY);
     TtyFb* fb = &tty->output.as_framebuffer;

     while(left) {
        assert(tty->input_kind == TTY_INPUT_KEYBOARD);
        for(;;) {
            if((e=vfs_read(&tty->input.as_kb.keyboard, &key, sizeof(key))) < 0) {
                if(fb->blink) ttyfb_fill_blink(fb, VGA_BG);
                return e;
            } else if (e > 0) break;
            size_t now = kernel.pit_info.ticks;
            if(now-fb->blink_time >= TTY_MILISECOND_BLINK) {
                fb->blink = !fb->blink;
                fb->blink_time = now;
                ttyfb_fill_blink(fb, blink_color[fb->blink]);
            }
        }
        key_set(&tty->input.as_kb.keystate, key.code, key.attribs);
        switch(key.code) {
        case MINOS_KEY_BACKSPACE: {
            if(!(key.attribs & KEY_ATTRIB_RELEASE)) {
                if(left != size) {
                    if(fb->blink) ttyfb_fill_blink(fb, VGA_BG);
                    fb->x -= 8;
                    ttyfb_fill_blink(fb, blink_color[fb->blink]);
                    left++;
                    buf--;
                }
            }
        } break;
        default: {
            int code = key_unicode(&tty->input.as_kb.keystate, key.code);
            if(code) {
                // NOTE: Non unicode keys are not yet supported cuz of reasons
                if(code >= 256) return -UNSUPPORTED;
                // NOTE: its fine to not clean it up but I do it just in case
                if(fb->blink) ttyfb_fill_blink(fb, VGA_BG);
                tty_draw_codepoint(fb, code, VGA_FG, VGA_BG);
                *((char*)buf) = code;
                if(left) ttyfb_fill_blink(fb, blink_color[fb->blink]);
                left--;
                buf++;
            }
            if(key.code == MINOS_KEY_ENTER && !(key.attribs & KEY_ATTRIB_RELEASE)) return size-left;
        } break;
        }
     }

     return size;
}


static intptr_t new_tty_private_display(void** private, size_t display, const char* keyboard) {
    ktrace("[new_tty_private] display (id=%zu)",display);
    ktrace("[new_tty_private] keyboard: %s",keyboard);
    debug_assert(private);
    debug_assert(keyboard);
    TtyDevice* tty = cache_alloc(tty_cache);
    intptr_t e;
    if(tty == NULL) return -NOT_ENOUGH_MEM;
    memset(tty, 0, sizeof(*tty));
    *private = tty;
    tty->output_kind = TTY_OUTPUT_DISPLAY;
    tty->output.as_framebuffer.fb = get_framebuffer_by_id(display);
    tty->output.as_framebuffer.x = 0;
    tty->output.as_framebuffer.y = 0;

    if(!tty->output.as_framebuffer.fb.addr) {
        cache_dealloc(tty_cache, tty);
        return -NOT_FOUND;
    }
    tty->input_kind = TTY_INPUT_KEYBOARD;
    if((e=vfs_open(keyboard, &tty->input.as_kb.keyboard, MODE_READ)) < 0) {
        kwarn("[new_tty_private] (vfs_open) Keyboard Error\n");
        cache_dealloc(tty_cache, tty);
        return e;
    }
    return 0;
}
intptr_t create_tty_device_display(size_t display, const char* keyboard, Device* device) {
    static_assert(sizeof(Device) == 32, "Update create_tty_device");
    intptr_t e = 0;
    device->ops = &ttyOps;
    device->open = tty_open;
    device->stat = NULL;
    if((e = new_tty_private_display(&device->private, display, keyboard)) < 0) return e;
    return 0;
}

void destroy_tty_device(Device* device) {
    if(!device || !device->private) return;
    TtyDevice* tty = device->private;
    vfs_close(&tty->input.as_kb.keyboard);
}

static intptr_t tty_init() {
    memset(&ttyOps, 0, sizeof(ttyOps));
    ttyOps.write = tty_dev_write;
    ttyOps.read = tty_dev_read;
    ttyOps.close = tty_close;
    tty_cache = create_new_cache(sizeof(TtyDevice), "TtyDevice");
    if(!tty_cache) return -NOT_ENOUGH_MEM;
    return 0;
}



void init_tty() {
    intptr_t e = 0;
    if((e = tty_init()) < 0) {
        kwarn("Failed to initialise VGA: %s",status_str(e));
        return;
    }
    size_t display = 0;
    const char* keyboard = "/devices/ps2keyboard";

    Device* device = (Device*)cache_alloc(kernel.device_cache);
    if(!device) return;
    e = create_tty_device_display(display, keyboard, device);
    if(e < 0) {
        kwarn("Failed to initialise TTY (id=%zu): %s",0lu,status_str(e));
        cache_dealloc(kernel.device_cache, device);
        return;
    }
    if((e = vfs_register_device("tty0", device)) < 0) {
        kwarn("Failed to register TTY device (id=%zu): %s",0lu,status_str(e));
        destroy_vga_device(device);
        cache_dealloc(kernel.device_cache, device);
        return;
    }
    ktrace("Initialised TTY (id=%zu)",0lu);
}
