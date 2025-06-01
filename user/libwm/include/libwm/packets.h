#pragma once
#include <stdint.h>
#include <assert.h>
#include <libwm.h>

#define SIZE_CHECK(T, n) static_assert(sizeof(T) == n, "Update " # T "_PACKET");
SIZE_CHECK(WmCreateWindowInfo, 40 + sizeof(char*));
#define WmCreateWindowInfo_PACKET \
    PCONST(uint32_t, x, u32) \
    PCONST(uint32_t, y, u32) \
    PCONST(uint32_t, width, u32) \
    PCONST(uint32_t, height, u32) \
    PCONST(uint32_t, min_width, u32) \
    PCONST(uint32_t, min_height, u32) \
    PCONST(uint32_t, max_width, u32) \
    PCONST(uint32_t, max_height, u32) \
    PCONST(uint32_t, flags, u32) \
    PCONST(uint32_t, title_len, u32) \
    PSTRING(title, title_len, 64)
