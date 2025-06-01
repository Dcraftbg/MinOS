#pragma once
#include <stdint.h>
#include <assert.h>
#include <libwm.h>

#define SIZE_CHECK(T, n) static_assert(sizeof(T) == n, "Update " # T "_PACKET");
SIZE_CHECK(WmCreateWindowInfo, 40 + sizeof(char*));
#define WmCreateWindowInfo_PACKET \
    PCONST(uint32_t, x) \
    PCONST(uint32_t, y) \
    PCONST(uint32_t, width) \
    PCONST(uint32_t, height) \
    PCONST(uint32_t, min_width) \
    PCONST(uint32_t, min_height) \
    PCONST(uint32_t, max_width) \
    PCONST(uint32_t, max_height) \
    PCONST(uint32_t, flags) \
    PCONST(uint32_t, title_len) \
    PSTRING(title, title_len, 64)

SIZE_CHECK(WmCreateSHMRegion, 8);
#define WmCreateSHMRegion_PACKET \
    PCONST(uint64_t, size)

SIZE_CHECK(WmDrawSHMRegion, 40);
#define WmDrawSHMRegion_PACKET \
    PCONST(uint32_t, window)\
    PCONST(uint32_t, window_x)\
    PCONST(uint32_t, window_y)\
    PCONST(uint32_t, shm_key)\
    PCONST(uint32_t, width)\
    PCONST(uint32_t, height)\
    PCONST(uint64_t, pitch_bytes)\
    PCONST(uint64_t, offset_bytes)

