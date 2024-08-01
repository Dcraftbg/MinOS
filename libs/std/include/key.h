#pragma once
#include "keycodes.h"
#include <stdint.h>
#define KEY_ATTRIB_RELEASE BIT(1)
typedef struct {
    uint16_t code : 12;
    uint8_t attribs : 4;
} Key;
#define MAKE_KEY(code, attribs) (Key) { code, attribs }
