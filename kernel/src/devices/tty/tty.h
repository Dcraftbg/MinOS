#pragma once
#include "../../vfs.h"
#include <minos/status.h>
#include "../../serial.h"
#include "../../string.h"
#include "../../mem/slab.h"
#include "../../framebuffer.h"
#include "../../bootutils.h"
#include <stdint.h>
#include <minos/tty/ttydefs.h>
typedef struct {
    char* data;
    size_t len, cap;
    char small_inline[128];
} TtyScratch;

bool ttyscratch_reserve(TtyScratch* scratch, size_t extra);
void ttyscratch_shrink(TtyScratch* scratch);
static inline void ttyscratch_init(TtyScratch* scratch) {
    scratch->data = scratch->small_inline;
    scratch->len = 0;
    scratch->cap = sizeof(scratch->small_inline);
}
static inline bool ttyscratch_push(TtyScratch* scratch, char c) {
    if(!ttyscratch_reserve(scratch, 1)) return false;
    scratch->data[scratch->len++] = c;
    return true;
}
static inline char ttyscratch_pop(TtyScratch* scratch) {
    if(scratch->len == 0) return 0;
    return scratch->data[--scratch->len];
}
static inline char ttyscratch_popfront(TtyScratch* scratch) {
    if(scratch->len == 0) return 0;
    char c = scratch->data[0];
    scratch->len--;
    memmove(scratch->data, scratch->data+1, scratch->len);
    return c;
}
typedef struct Tty Tty;
struct Tty {
    TtyScratch scratch;
    ttyflags_t flags;
    void* priv;
    uint32_t (*getchar)(Tty* device);
    void     (*putchar)(Tty* device, uint32_t code);
    intptr_t (*deinit )(Tty* device);
};
void init_tty(void);
Tty* tty_new(void);
Device* create_tty_device(Tty* tty);
