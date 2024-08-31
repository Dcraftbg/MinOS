#include "tty.h"
#include "../../print.h"
#include "../../debug.h"
#include "../../kernel.h"
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
typedef struct {
     VfsFile keyboard;
     Keyboard keystate;

     VfsFile display;
     size_t width, height;
     size_t x, y;
} TtyDevice;
static Cache* tty_cache = NULL;
static intptr_t tty_open(struct Device* this, VfsFile* file, fmode_t mode) {
     file->private = this->private;
     return 0;
}

static void tty_close(VfsFile* file) {
    file->private = NULL;
}

static intptr_t tty_dev_write(VfsFile* file, const void* buf, size_t size, off_t offset) {
     (void)offset;
     TtyDevice* tty = (TtyDevice*)file->private;
     return vfs_write(&tty->display, buf, size);
}
// TODO: Unicode support
static intptr_t tty_dev_read(VfsFile* file, void* buf, size_t size, off_t offset) {
     (void)offset;
     TtyDevice* tty = (TtyDevice*)file->private;
     intptr_t e;
     Key key;
     size_t left=size;
     while(left) {
        for(;;) {
            if((e=vfs_read(&tty->keyboard, &key, sizeof(key))) < 0) {
                return e;
            } else if (e > 0) break;
        }

        key_set(&tty->keystate, key.code, key.attribs);
        int code = key_unicode(&tty->keystate, key.code);
        if(code) {
            // NOTE: Non unicode keys are not yet supported cuz of reasons
            if(code >= 256) return -UNSUPPORTED; 
            if((e = write_exact(&tty->display, &code, 1)) < 0) {
                return e;
            }
            *((char*)buf) = code;
            left--;
            buf++;
        }
     }
     return size;
}


static intptr_t new_tty_private(void** private, const char* display, const char* keyboard) {
    printf("TRACE: [new_tty_private] display: %s\n",display);
    printf("TRACE: [new_tty_private] keyboard: %s\n",keyboard);
    debug_assert(private);
    debug_assert(display);
    debug_assert(keyboard);
    TtyDevice* tty = cache_alloc(tty_cache);
    intptr_t e;
    if(tty == NULL) return -NOT_ENOUGH_MEM;
    memset(tty, 0, sizeof(*tty));
    *private = tty;
    VfsStats stats;
    VfsDirEntry displayEntry;
    if((e=vfs_find(display, &displayEntry)) < 0) {
        printf("  [new_tty_private] (vfs_find) Error\n");
        cache_dealloc(tty_cache, tty);
        return e;
    }
    if((e=vfs_stat(&displayEntry, &stats)) < 0) {
        printf("  [new_tty_private] (vfs_stat) Error\n");
        cache_dealloc(tty_cache, tty);
        return e;
    }
    tty->width = stats.width;
    tty->height = stats.height;
    if((e=vfs_open(display, &tty->display, MODE_WRITE)) < 0) {
        printf("  [new_tty_private] (vfs_open) Display Error\n");
        cache_dealloc(tty_cache, tty);
        return e;
    }
    if((e=vfs_open(keyboard, &tty->keyboard, MODE_READ)) < 0) {
        printf("  [new_tty_private] (vfs_open) Keyboard Error\n");
        cache_dealloc(tty_cache, tty);
        vfs_close(&tty->display);
        return e;
    }
    return 0;
}
intptr_t create_tty_device(const char* display, const char* keyboard, Device* device) {
    static_assert(sizeof(Device) == 48, "Update create_tty_device");
    intptr_t e = 0;
    device->ops = &ttyOps;
    device->open = tty_open;
    device->init = tty_init;
    device->deinit = NULL;
    device->stat = NULL;
    if((e = new_tty_private(&device->private, display, keyboard)) < 0) return e;
    return 0;
}

void destroy_tty_device(Device* device) {
    if(!device || !device->private) return;
    TtyDevice* tty = device->private;
    vfs_close(&tty->keyboard);
    vfs_close(&tty->display);
}

intptr_t tty_init() {
    memset(&ttyOps, 0, sizeof(ttyOps));
    ttyOps.write = tty_dev_write;
    ttyOps.read = tty_dev_read;
    ttyOps.close = tty_close;
    tty_cache = create_new_cache(sizeof(TtyDevice), "TtyDevice");
    if(!tty_cache) return -NOT_ENOUGH_MEM;
    return 0;
}
