#include "ctype.h"
#include <stdint.h>
// Bitmap of:
//  " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
static uint8_t ISPRINT_CHARS_MAP[16 /*128 bits*/] = {
   0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F
};
int isprint(int c) {
    return c < 0 || c/8 >= sizeof(ISPRINT_CHARS_MAP) ? 0 : (ISPRINT_CHARS_MAP[c/8] & 1<<(c%8));
}
