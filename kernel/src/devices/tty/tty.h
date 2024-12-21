#pragma once
#include "../../vfs.h"
#include <minos/status.h>
#include "../../serial.h"
#include "../../string.h"
#include "../../mem/slab.h"
#include "../../framebuffer.h"
#include "../../bootutils.h"
#include <stdint.h>
typedef struct {
    char* data;
    size_t len, cap;
    char small_inline[128];
} TtyScratch;
typedef struct Tty Tty;
struct Tty {
    TtyScratch scratch;
    void* priv;
    uint32_t (*getchar)(Tty* device);
    void     (*putchar)(Tty* device, uint32_t code);
    intptr_t (*deinit )(Tty* device);
};
void init_tty(void);
Tty* tty_new(void);
Device* create_tty_device(Tty* tty);
