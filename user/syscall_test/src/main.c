#include <minos/sysstd.h>
#include <stdbool.h>
#define HALT() \
    for(;;) 

uintptr_t stdout = 0;
uintptr_t keyboard = 0;
#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "../vendor/stb_sprintf.h"
#include <minos/keycodes.h>
typedef struct {
    uint16_t code    : 12;
    uint8_t  attribs : 4;
} Key;


#define PRINTF_TMP 1024
static char tmp_printf[PRINTF_TMP]={0};

size_t strlen(const char* cstr) {
    const char* head = cstr;
    while(*head) head++;
    return head-cstr;
}
void printf(const char* fmt, ...) __attribute__((format(printf,1, 2)));
void printf(const char* fmt, ...) {
   va_list va;
   va_start(va, fmt);
   size_t n = stbsp_vsnprintf(tmp_printf, PRINTF_TMP, fmt, va);
   va_end(va);
   write(stdout, tmp_printf, n);
}
#define KEY_BYTES ((MINOS_KEY_COUNT+7)/8)
typedef struct {
   // Is key down?
   uint8_t state[KEY_BYTES];
} Keyboard;
Keyboard kb_state={0};
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
uint8_t US_QWERTY_SHIFTED[256] = {
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
int key_unicode(uint16_t code) {
    switch(code) {
    case MINOS_KEY_ENTER:
        if(key_get(&kb_state, MINOS_KEY_ENTER)) return '\n';
        return 0;
    default:
        if(code >= 256 || !key_get(&kb_state, code)) return 0;
        if(key_get(&kb_state, MINOS_KEY_LEFT_SHIFT) || key_get(&kb_state, MINOS_KEY_RIGHT_SHIFT)) {
            return US_QWERTY_SHIFTED[code];
        }
        if(code >= 'A' && code <= 'Z') return code-'A' + 'a';
        return code;
    }
}
int main() {
    intptr_t e = 0;
    printf("Hello World!\n");
    printf("If you're seeing this it means that you successfully booted into user mode! :D\n");
    for(;;) {
        Key key = {0};
        if((e=read(keyboard, &key, sizeof(key))) < 0) {
            printf("Failed to read ps2 keyboard!\n");
            HALT();
        }
        if(e != 0) {
            key_set(&kb_state, key.code, key.attribs);
            int code = key_unicode(key.code);
            if(code) printf("%c",code);
        }
    }
    return 0;
}
void _start(int argc, const char** argv) {
    if((stdout = open("/devices/vga0", MODE_WRITE)) < 0) HALT();
    printf("Args dump:\n");
    for(size_t i = 0; i < argc; ++i) {
        printf("%zu> ",i+1); printf("%p",argv[i]); printf(" %s\n",argv[i]);
    }
    if((keyboard = open("/devices/ps2keyboard", MODE_READ)) < 0) {
        printf("Failed to initialise ps2 keyboard!\n");
        HALT();
    }
    main();
    close(stdout);
    HALT();
}
