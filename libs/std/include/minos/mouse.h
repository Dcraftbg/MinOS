#pragma once
#include <stdint.h>
enum {
    MOUSE_EVENT_KIND_MOVE,
    MOUSE_EVENT_KIND_BUTTON,
};
#define MOUSE_BUTTON_ON_MASK   (0x80000000)
#define MOUSE_BUTTON_CODE_MASK (0x7FFFFFFF)

enum {
    MOUSE_BUTTON_CODE_LEFT,
    MOUSE_BUTTON_CODE_MIDDLE,
    MOUSE_BUTTON_CODE_RIGHT,
};
typedef struct {
    uint32_t kind;
    union {
        struct {
            int32_t delta_x;
            int32_t delta_y;
        } move;
        // [ 1 bit ][ 31 bits ]
        // [  on   ][  code   ]
        uint32_t button;
    } as;
} MouseEvent;
