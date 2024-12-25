#include "fb.h"
#include "../../kernel.h"
#include "../../fbwriter.h"
#include "../../log.h"
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
static int key_unicode(KeyState* keyboard, uint16_t code) {
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
#define MAX_CSI_NUMS 5
typedef struct {
    int nums[MAX_CSI_NUMS];
    size_t current_num;
} CsiParser;
typedef struct {
    Inode* keyboard;
    KeyState keystate;
    FbTextWriter fbt;
    bool blink;
    size_t blink_time;
    enum {
        STATE_NORMAL,
        STATE_ANSI_ESCAPE,
        STATE_CSI,
    } state;
    CsiParser csi;
    uint32_t fg, bg;
} FbTty;
static Cache* fbtty_cache = NULL;
intptr_t init_fbtty(void) {
    if(!(fbtty_cache = create_new_cache(sizeof(FbTty), "fbtty_cache")))
        return -NOT_ENOUGH_MEM;
    return 0;
}
void deinit_fbtty(void) {
    // FIXME: Memory leak in here.
    // TODO: Implement cache_destory and remove this cache
    fbtty_cache = NULL;
}
static FbTty* fbtty_new_internal(Inode* keyboard, FbTextWriter writer) {
    FbTty* tty = cache_alloc(fbtty_cache);
    if(!tty) return tty;
    memset(tty, 0, sizeof(*tty));
    tty->keyboard = keyboard;
    tty->fbt = writer;
    tty->fg = VGA_FG;
    tty->bg = VGA_BG;
    return tty;
}

static void fbtty_fill_blink(FbTty* fb, uint32_t color) {
    size_t x=fb->fbt.x, y=fb->fbt.y;
    fbwriter_draw_codepoint(&fb->fbt, CODE_BLOCK, color, fb->bg);
    fb->fbt.x = x; 
    fb->fbt.y = y;
}

#define TTY_MILISECOND_BLINK 500
static uint32_t fbtty_getchar(Tty* device) {
    intptr_t e;
    FbTty* fbtty = device->priv;
    uint32_t code;
    Key key;
    for(;;) {
        // TODO: Thread blocker for this stuff
        for(;;) {
            if((e=inode_read(fbtty->keyboard, &key, sizeof(key), 0)) < 0) {
                if(fbtty->blink) fbtty_fill_blink(fbtty, fbtty->bg);
                return e;
            } else if (e > 0) break;
            size_t now = kernel.pit_info.ticks;
            if(now-fbtty->blink_time >= TTY_MILISECOND_BLINK) {
                fbtty->blink = !fbtty->blink;
                fbtty->blink_time = now;
                fbtty_fill_blink(fbtty, fbtty->blink ? fbtty->fg : fbtty->bg);
            }
        }
        key_set(&fbtty->keystate, key.code, key.attribs);
        code = key_unicode(&fbtty->keystate, key.code);
        if(!(key.attribs & KEY_ATTRIB_RELEASE)) break; 
    }
    if(code == '\n') {
        fbtty->blink = false;
        fbtty_fill_blink(fbtty, fbtty->blink ? fbtty->fg : fbtty->bg);
    }
    return code;
}
static void handle_csi_final(FbTty* tty, uint32_t code) {
    switch(code) {
    default:
        kerror("(fbtty) Unsupported csi final code: %c (%02X)", code, code);
        break;
    }
}
static void fbtty_putchar(Tty* device, uint32_t code) {
    FbTty* fbtty = device->priv;
    switch(fbtty->state) {
    case STATE_NORMAL:
        if(code == 0x1B) {
            fbtty->state = STATE_ANSI_ESCAPE;
            break;
        }
        if(fbtty->blink) fbtty_fill_blink(fbtty, fbtty->bg);
        fbwriter_draw_codepoint(&fbtty->fbt, code, fbtty->fg, fbtty->bg);
        fbtty_fill_blink(fbtty, fbtty->blink ? fbtty->fg : fbtty->bg);
        break;
    case STATE_ANSI_ESCAPE:
        switch(code) {
        case '[':
            fbtty->state = STATE_CSI;
            break;
        default:
            kerror("(fbtty) Unsupported escape character %c (%02X)", code, code);
            fbtty->state = STATE_NORMAL;
        }
        break;
    case STATE_CSI:
        switch(code) {
        case ';': {
            if(++fbtty->csi.current_num >= MAX_CSI_NUMS) {
                kerror("(fbtty) Too many arguments in ANSI escape sequence");
                fbtty->csi.current_num = 0;
                fbtty->state = STATE_NORMAL;
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
            fbtty->csi.nums[fbtty->csi.current_num] *= 10;
            fbtty->csi.nums[fbtty->csi.current_num] += code-'0';
            break;
        default:
            // We reached the final byte
            if(code >= 0x40 && code <= 0x7E) {
                handle_csi_final(fbtty, code);
            } else {
                kerror("(fbtty) Unsupported code in csi escape sequence: %c (%02X)", code, code);
            }
            fbtty->state = STATE_NORMAL;
            fbtty->csi.current_num = 0;
            break;
        }
        }
        break;
    default:
        kerror("Invalid state: %d", fbtty->state);
    }
}
static intptr_t fbtty_deinit(Tty* device) {
    FbTty* fbtty = device->priv;
    idrop(fbtty->keyboard);
    cache_dealloc(fbtty_cache, fbtty);
    return 0;
}
Tty* fbtty_new(Inode* keyboard, size_t framebuffer_id) {
    FbTextWriter writer={0};
    writer.fb = get_framebuffer_by_id(framebuffer_id);
    if(!writer.fb.addr) {
        kerror("(fbtty) Cannot create fbtty on framebuffer#%zu Framebuffer not existent", framebuffer_id);
        return NULL;
    }
    if(writer.fb.bpp != 32) {
        kerror("(fbtty) Cannot create fbtty with bpp != 32 (bpp=%zu for framebuffer#%zu)", writer.fb.bpp, framebuffer_id);
        return NULL;
    }
    FbTty* fbtty = fbtty_new_internal(keyboard, writer);
    if(!fbtty) return NULL;
    Tty* tty = tty_new();
    if(!tty) {
        cache_dealloc(fbtty_cache, fbtty);
        return NULL;
    }
    tty->priv = fbtty;
    tty->putchar = fbtty_putchar;
    tty->getchar = fbtty_getchar;
    tty->deinit = fbtty_deinit;
    return tty;
}
