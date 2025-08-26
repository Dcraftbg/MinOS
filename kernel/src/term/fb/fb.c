#include "fb.h"
#include <devices/tty/tty.h>
#include <timer.h>
#include <fbwriter.h>
#include <log.h>
#include <minos/keycodes.h>
#include <minos/key.h>
#define KEY_BYTES ((MINOS_KEY_COUNT+7)/8)
typedef struct {
    // Is key down?
    uint8_t state[KEY_BYTES];
} KeyState;
static void key_set(KeyState* kb, uint16_t code, uint8_t released) {
    uint8_t down = released == 0;
    //debug_assert(code < ARRAY_LEN(kb->state));
    kb->state[code/8] &= ~(1 << (code % 8));
    kb->state[code/8] |= down << (code%8);
}
static bool key_get(KeyState* kb, uint16_t code) {
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
static bool key_extend(KeyState* keyboard, TtyScratch* scratch, uint16_t code) {
    switch(code) {
    case MINOS_KEY_ENTER:
        if(key_get(keyboard, MINOS_KEY_ENTER))
            return ttyscratch_push(scratch, '\n');
        break;
    case MINOS_KEY_UP_ARROW:
    case MINOS_KEY_DOWN_ARROW:
    case MINOS_KEY_RIGHT_ARROW:
    case MINOS_KEY_LEFT_ARROW: {
        if(!ttyscratch_reserve(scratch, 3)) return false;
        ttyscratch_push(scratch, 0x1b);
        ttyscratch_push(scratch, '[');
        ttyscratch_push(scratch, (code - MINOS_KEY_UP_ARROW) + 'A');
    } break;
    default:
        if(code >= 256 || !key_get(keyboard, code)) 
            break; 
        if(key_get(keyboard, MINOS_KEY_LEFT_SHIFT) || key_get(keyboard, MINOS_KEY_RIGHT_SHIFT))
            return ttyscratch_push(scratch, US_QWERTY_SHIFTED[code]);
        if(code >= 'A' && code <= 'Z') 
            return ttyscratch_push(scratch, code-'A' + 'a');
        return ttyscratch_push(scratch, code);
    }
    return true;
}
#define MAX_CSI_NUMS 5
typedef struct {
    int nums[MAX_CSI_NUMS];
    size_t nums_count;
} CsiParser;
typedef struct {
    char c;
    uint32_t fg, bg;
} Codepoint;
typedef struct {
    Tty tty;
    Inode* keyboard;
    TtyScratch scratch;
    KeyState keystate;
    Framebuffer fb;
    size_t x, y;
    size_t w, h;
    Codepoint* map;
    enum {
        STATE_NORMAL,
        STATE_ANSI_ESCAPE,
        STATE_CSI,
    } state;
    CsiParser csi;
    uint32_t cursor_color;
    uint32_t fg, bg;
} FbTty;
static Cache* fbtty_cache = NULL;
intptr_t init_fbtty(void) {
    if(!(fbtty_cache = create_new_cache(sizeof(FbTty), "FbTty")))
        return -NOT_ENOUGH_MEM;
    return 0;
}
void deinit_fbtty(void) {
    // FIXME: Memory leak in here.
    // TODO: Implement cache_destory and remove this cache
    fbtty_cache = NULL;
}

static uint32_t fbtty_getchar(Tty* device);
static void fbtty_putchar(Tty* device, uint32_t code);
static intptr_t fbtty_getsize(Tty* device, TtySize* size);
static intptr_t fbtty_deinit(Tty* device);

static void fbtty_fill_blink(FbTty* fb, bool on);

#define TTY_MILISECOND_BLINK 500
static bool fbtty_is_readable(Inode* device) {
    FbTty* fbtty = (FbTty*)device;
    if(fbtty->scratch.len > 0 || inode_is_readable(fbtty->keyboard)) return true;
    return false;
}
static InodeOps inodeOps = {
    .write = tty_write,
    .read = tty_read,
    .ioctl = tty_ioctl,
    .is_readable = fbtty_is_readable,
};
static FbTty* fbtty_new_internal(Inode* keyboard, Framebuffer fb) {
    FbTty* tty = cache_alloc(fbtty_cache);
    if(!tty) return NULL;
    memset(tty, 0, sizeof(*tty));
    tty_init(&tty->tty, fbtty_cache);
    tty->tty.putchar = fbtty_putchar;
    tty->tty.getchar = fbtty_getchar;
    tty->tty.getsize = fbtty_getsize;
    tty->tty.deinit = fbtty_deinit;
    tty->tty.inode.ops = &inodeOps;
    tty->w  = fb.width/8;
    tty->h  = fb.height/16;
    tty->map = kernel_malloc(tty->w * tty->h * sizeof(tty->map[0]));
    if(!tty->map) {
        cache_dealloc(fbtty_cache, tty);
        return NULL;
    }
    tty->cursor_color = VGA_FG;
    tty->fg = VGA_FG;
    tty->bg = VGA_BG;
    size_t chars = tty->w * tty->h;
    for(size_t i = 0; i < chars; ++i) {
        tty->map[i].c = ' ';
        tty->map[i].fg = tty->fg;
        tty->map[i].bg = tty->bg;
    }
    tty->keyboard = keyboard;
    tty->fb = fb;
    ttyscratch_init(&tty->scratch);
    return tty;
}

static void fbtty_fill_blink(FbTty* fb, bool on) {
    Codepoint* cpt = &fb->map[fb->y * fb->w + fb->x];
    uint32_t fg = cpt->fg, bg = cpt->bg;
    if(on) {
        bg = fb->cursor_color;
        fg = 0xffffffff - cpt->fg;
    }
    fb_draw_codepoint_at(&fb->fb, fb->x*8, fb->y*16, cpt->c, fg, bg);
}
static intptr_t fbtty_getsize(Tty* device, TtySize* size) {
    FbTty* fbtty = (FbTty*)device;
    size->width  = fbtty->w;
    size->height = fbtty->h;
    return 0;
}
static uint32_t fbtty_getchar(Tty* device) {
    FbTty* fbtty = (FbTty*)device;
    // TODO: Potentially in the future we would collect all the key codes 
    // so that we aren't dependent on the keyboard keeping up with its circular buffer
    // but this works for now.
    // IF YOU GO ABOUT THIS, CHECK ALLOCATION FAILURE

    while(fbtty->scratch.len == 0) {
        intptr_t e = 0;
        Key key;
        while((e=inode_read(fbtty->keyboard, &key, sizeof(key), 0)) > 0) {
            key_set(&fbtty->keystate, key.code, key.attribs);
            // FIXME: Ignores memory alloc failure. Shouldn't happen really
            // Cuz the scratch buffer is never going to actually allocate any memory but yk
            // Still leaving this comment just in case this becomes an issue in the future
            if(!(key.attribs & KEY_ATTRIB_RELEASE)) 
                key_extend(&fbtty->keystate, &fbtty->scratch, key.code);
        }
    }
    char res = ttyscratch_popfront(&fbtty->scratch);
    ttyscratch_shrink(&fbtty->scratch);
    return res;
}
static uint32_t bit4_colors[8] = {
#ifdef JAKE_COLORSCHEME
    // NOTE: Soft colors. Really cool :D
    0xffa4ada6,
    0xfff07473,
    0xff00aa00,
    0xffdaa05c,
    0xff0000aa,
    0xffa48abc,
    0xff447ff0,
    0xffaaaaaa
#else
    0xff000000,
    0xffaa0000,
    0xff00aa00,
    0xffaa5500,
    0xff0000aa,
    0xffaa00aa,
    0xff00aaaa,
    0xffaaaaaa
#endif
};
static uint32_t bit4_bold_colors[8] = {
    0xff555555,
    0xffff5555,
    0xff55ff55,
    0xffffff55,
    0xff5555ff,
    0xffff55ff,
    0xff55ffff,
    0xffffffff
};
static void handle_csi_final(FbTty* fbtty, uint32_t code) {
    switch(code) {
    case 'm': {
        int n = fbtty->csi.nums[0];
        switch(n) {
        case 0:
            fbtty->fg = VGA_FG;
            fbtty->bg = VGA_BG;
            break;
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            fbtty->fg = bit4_colors[n - 30];
            break;
        case 38:
            if(fbtty->csi.nums[1] == 2) {
                int r = fbtty->csi.nums[2];
                int g = fbtty->csi.nums[3];
                int b = fbtty->csi.nums[4];
                fbtty->fg = (0xFF << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF) << 0);
            } else {
                kerror("Unsupported non-RGB color escape");
            }
            break;
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            fbtty->bg = bit4_colors[n - 40];
            break;
        case 48:
            if(fbtty->csi.nums[1] == 2) {
                int r = fbtty->csi.nums[2];
                int g = fbtty->csi.nums[3];
                int b = fbtty->csi.nums[4];
                fbtty->bg = (0xFF << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF) << 0);
            } else {
                kerror("Unsupported non-RGB color escape");
            }
            break;
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            fbtty->fg = bit4_bold_colors[n - 90];
            break;
        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            fbtty->bg = bit4_bold_colors[n - 100];
            break;
        default:
            kerror("(fbtty) Unsupported SGR param: %d", n);
            break;
        }
    } break;
    case 'H': {
        fbtty_fill_blink(fbtty, false);
        int y=1, x=1;
        if(fbtty->csi.nums_count > 0) y = fbtty->csi.nums[0];
        if(fbtty->csi.nums_count > 1) x = fbtty->csi.nums[1];
        if(x <= 0) x = 1;
        if(y <= 0) y = 1;
        fbtty->x = (x-1);
        fbtty->y = (y-1);
        if(fbtty->x > fbtty->w) fbtty->x = fbtty->w-1;
        if(fbtty->y > fbtty->h) fbtty->y = fbtty->h-1;
        fbtty_fill_blink(fbtty, true);
    } break;
    case 'J': {
        int mode = fbtty->csi.nums_count > 0 ? fbtty->csi.nums[0] : 0;
        switch(mode) {
        case 2: {
            size_t chars = fbtty->h * fbtty->w;
            for(size_t i = 0; i < chars; ++i) {
                fbtty->map[i].c = ' ';
                fbtty->map[i].fg = fbtty->fg;
                fbtty->map[i].bg = fbtty->bg;
            }
            fmbuf_fill(&fbtty->fb, fbtty->bg);
        } break;
        default:
            kerror("(fbtty) Clear mode not supported (csi J)");
            break;
        }
    } break;
    case 'A':
    case 'B': {
        int dy = fbtty->csi.nums_count > 0 ? fbtty->csi.nums[0] : 0;
        if(dy == 0) break;
        fbtty_fill_blink(fbtty, false);
        fbtty->y = (int)fbtty->y + ((code - 'B') * 2 + 1) * dy;
        if(fbtty->y >= fbtty->h) fbtty->y = fbtty->h - 1;
        fbtty_fill_blink(fbtty, true);
    } break;
    case 'C':
    case 'D': {
        int dx = fbtty->csi.nums_count > 0 ? fbtty->csi.nums[0] : 0; 
        if(dx == 0) break;
        fbtty_fill_blink(fbtty, false);
        fbtty->x = (int)fbtty->x + (-((code - 'D') * 2 + 1)) * dx;
        if(fbtty->x >= fbtty->w) fbtty->x = fbtty->w - 1;
        fbtty_fill_blink(fbtty, true);
    } break;
    default:
        kerror("(fbtty) Unsupported csi final code: %c (%02X)", code, code);
        break;
    }
}
static void fbtty_draw_char(FbTty* fbtty, uint32_t code) {
    fbtty_fill_blink(fbtty, false);
    while(fbtty->x >= fbtty->w) {
        fbtty->x -= fbtty->w;
        fbtty->y++;
    }
    while(fbtty->y >= fbtty->h) {
        fmbuf_scroll_up(&fbtty->fb, 16, fbtty->bg);
        memmove(fbtty->map, fbtty->map+fbtty->w, fbtty->w*(fbtty->h-1)*sizeof(fbtty->map[0]));
        for(size_t i = 0; i < fbtty->w; ++i) {
            Codepoint* cpt = &(fbtty->map + fbtty->w*(fbtty->h-1))[i];
            cpt->c = ' ';
            cpt->fg = fbtty->fg;
            cpt->bg = fbtty->bg;
        }
        fbtty->y--;
    }
    size_t i = fbtty->y * fbtty->w + fbtty->x;
    switch(code) {
    case '\b': {
        if(fbtty->x) fbtty->x--;
        fbtty->map[fbtty->y * fbtty->w + fbtty->x].c = ' ';
        size_t x = fbtty->x * 8;
        size_t y = fbtty->y * 16;
        fmbuf_draw_rect(&fbtty->fb, x, y, x + 8, y + 16, fbtty->bg);
    } break;
    case ' ': {
        fbtty->map[i].c = code;
        fbtty->map[i].fg = fbtty->fg;
        fbtty->map[i].bg = fbtty->bg;
        size_t x = fbtty->x * 8;
        size_t y = fbtty->y * 16;
        fmbuf_draw_rect(&fbtty->fb, x, y, x + 8, y + 16, fbtty->bg);
        fbtty->x++;
    } break;
    case '\n':
        fbtty->map[i].c = ' ';// code;
        fbtty->map[i].fg = fbtty->fg;
        fbtty->map[i].bg = fbtty->bg;
        fbtty->y++;
        fbtty->x = 0;
        break;
    case '\t':
        fbtty->map[i].c = code;
        fbtty->x += 4;
        break;
    default:
        fbtty->map[i].c = code;
        fbtty->map[i].fg = fbtty->fg;
        fbtty->map[i].bg = fbtty->bg;
        fb_draw_codepoint_at(&fbtty->fb, fbtty->x * 8, fbtty->y * 16, code, fbtty->fg, fbtty->bg);
        fbtty->x++;
        break;
    }
    fbtty_fill_blink(fbtty, true);
}
static void fbtty_putchar(Tty* device, uint32_t code) {
    mutex_lock(&device->mutex);
    FbTty* fbtty = (FbTty*)device;
    switch(fbtty->state) {
    case STATE_NORMAL:
        if(code == 0x1B) {
            fbtty->state = STATE_ANSI_ESCAPE;
            break;
        }
        fbtty_draw_char(fbtty, code);
        break;
    case STATE_ANSI_ESCAPE:
        switch(code) {
        case '[':
            fbtty->state = STATE_CSI;
            memset(fbtty->csi.nums, 0, sizeof(fbtty->csi.nums));
            fbtty->csi.nums_count = 1;
            break;
        default:
            kerror("(fbtty) Unsupported escape character %c (%02X)", code, code);
            fbtty->state = STATE_NORMAL;
        }
        break;
    case STATE_CSI:
        switch(code) {
        case ';': {
            if(fbtty->csi.nums_count++ >= MAX_CSI_NUMS) {
                kerror("(fbtty) Too many arguments in ANSI escape sequence");
                fbtty->csi.nums_count = 0;
                fbtty->state = STATE_NORMAL;
                memset(fbtty->csi.nums, 0, MAX_CSI_NUMS*sizeof(fbtty->csi.nums[0]));
                mutex_unlock(&device->mutex);
                return;
            }
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            fbtty->csi.nums[fbtty->csi.nums_count-1] *= 10;
            fbtty->csi.nums[fbtty->csi.nums_count-1] += code-'0';
            break;
        default:
            // We reached the final byte
            if(code >= 0x40 && code <= 0x7E) {
                handle_csi_final(fbtty, code);
            } else {
                kerror("(fbtty) Unsupported code in csi escape sequence: %c (%02X)", code, code);
            }
            fbtty->state = STATE_NORMAL;
            fbtty->csi.nums_count = 0;
            memset(fbtty->csi.nums, 0, MAX_CSI_NUMS*sizeof(fbtty->csi.nums[0]));
            break;
        }
        }
        break;
    default:
        kerror("Invalid state: %d", fbtty->state);
    }
    mutex_unlock(&device->mutex);
}
static intptr_t fbtty_deinit(Tty* device) {
    FbTty* fbtty = (FbTty*)device;
    idrop(fbtty->keyboard);
    cache_dealloc(fbtty_cache, fbtty);
    return 0;
}
Tty* fbtty_new(Inode* keyboard, size_t framebuffer_id) {
    Framebuffer fb = get_framebuffer_by_id(framebuffer_id);
    if(!fb.addr) {
        kerror("(fbtty) Cannot create fbtty on framebuffer#%zu Framebuffer not existent", framebuffer_id);
        return NULL;
    }
    if(fb.bpp != 32) {
        kerror("(fbtty) Cannot create fbtty with bpp != 32 (bpp=%zu for framebuffer#%zu)", fb.bpp, framebuffer_id);
        return NULL;
    }
    FbTty* fbtty = fbtty_new_internal(keyboard, fb);
    return &fbtty->tty;
}
