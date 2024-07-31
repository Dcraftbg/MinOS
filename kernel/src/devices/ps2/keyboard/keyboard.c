#include "keyboard.h"
static uint16_t SCAN1_PS2[] = {
    0   ,MINOS_KEY_ESCAPE,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,'0' ,'-' ,'=' ,MINOS_KEY_BACKSPACE, MINOS_KEY_TAB,
    'Q' ,'W' ,'E' ,'R' ,'T' ,'Y' ,'U' ,'I' ,'O' ,'P' ,'[' ,']' , MINOS_KEY_ENTER, MINOS_KEY_LEFT_CTRL, 'A' ,'S' ,
    'D' ,'F' ,'G' ,'H' ,'J' ,'K' ,'L' ,';' ,'\'','`' ,MINOS_KEY_LEFT_SHIFT ,'\\' ,'Z' ,'X' ,'C' ,'V' ,
    'B' ,'N' ,'M' ,',' ,'.' ,'/' ,MINOS_KEY_RIGHT_SHIFT ,MINOS_KEY_NUMPAD_STAR ,MINOS_KEY_LEFT_ALT ,' ' ,MINOS_KEY_CAPSLOCK ,MINOS_KEY_F1 ,MINOS_KEY_F2 ,MINOS_KEY_F3 ,MINOS_KEY_F4 ,MINOS_KEY_F5 ,
    MINOS_KEY_F6 ,MINOS_KEY_F7 ,MINOS_KEY_F8 ,MINOS_KEY_F9 ,MINOS_KEY_F10 ,MINOS_KEY_NUMLOCK ,MINOS_KEY_SCROLLLOCK ,MINOS_KEY_NUMPAD_7 ,MINOS_KEY_NUMPAD_8 ,MINOS_KEY_NUMPAD_9 ,MINOS_KEY_NUMPAD_MINUS ,MINOS_KEY_NUMPAD_4 ,MINOS_KEY_NUMPAD_5 ,MINOS_KEY_NUMPAD_6 ,MINOS_KEY_NUMPAD_PLUS ,MINOS_KEY_NUMPAD_1 ,
    MINOS_KEY_NUMPAD_2 ,MINOS_KEY_NUMPAD_3 ,MINOS_KEY_NUMPAD_0 ,MINOS_KEY_NUMPAD_DOT ,0   ,0   ,0   ,MINOS_KEY_F11 ,MINOS_KEY_F12 ,0   ,0   ,0   ,0   ,0   ,0   ,0   ,
};
static void* max_memcpy(void* dest, const void* src, size_t destcap, size_t srclen) {
    if(destcap < srclen) srclen = destcap;
    return memcpy(dest, src, srclen);
}
static void* max_strcpy(void* dest, const char* src, size_t destcap) {
    return max_memcpy(dest, src, destcap, strlen(src)+1);
}
static const char* keycode_display(uint16_t key, char* buf, size_t size) {
    switch(key) {
        case MINOS_KEY_F1:
        case MINOS_KEY_F2:
        case MINOS_KEY_F3:
        case MINOS_KEY_F4:
        case MINOS_KEY_F5:
        case MINOS_KEY_F6:
        case MINOS_KEY_F7:
        case MINOS_KEY_F8:
        case MINOS_KEY_F9:
        case MINOS_KEY_F10:
        case MINOS_KEY_F11:
        case MINOS_KEY_F12:
            stbsp_snprintf(buf, size, "F%u",key-(MINOS_KEY_F1-1));
            break;
        case MINOS_KEY_NUMPAD_0:
        case MINOS_KEY_NUMPAD_1:
        case MINOS_KEY_NUMPAD_2:
        case MINOS_KEY_NUMPAD_3:
        case MINOS_KEY_NUMPAD_4:
        case MINOS_KEY_NUMPAD_5:
        case MINOS_KEY_NUMPAD_6:
        case MINOS_KEY_NUMPAD_7:
        case MINOS_KEY_NUMPAD_8:
        case MINOS_KEY_NUMPAD_9:
            stbsp_snprintf(buf, size, "F%u",key-MINOS_KEY_NUMPAD_0);
            break;
        case MINOS_KEY_NUMPAD_SLASH:
            max_strcpy(buf, "/", size);
            break;
        case MINOS_KEY_NUMPAD_STAR:
            max_strcpy(buf, "*", size);
            break;

        case MINOS_KEY_NUMPAD_MINUS:
            max_strcpy(buf, "-", size);
            break;
        case MINOS_KEY_NUMPAD_PLUS:
            max_strcpy(buf, "+", size);
            break;
        case MINOS_KEY_NUMPAD_DOT:
            max_strcpy(buf, ".", size);
            break;

        case MINOS_KEY_BACKSPACE:
            max_strcpy(buf, "Backspace", size);
            break;
        case MINOS_KEY_TAB:
            max_strcpy(buf, "\t", size);
            break;
        case MINOS_KEY_ESCAPE:
            max_strcpy(buf, "Escape", size);
            break;
        case MINOS_KEY_LEFT_CTRL:
            max_strcpy(buf, "Left Control", size);
            break;
        case MINOS_KEY_LEFT_SHIFT:
            max_strcpy(buf, "Left Shift", size);
            break;
        case MINOS_KEY_RIGHT_SHIFT:
            max_strcpy(buf, "Right Shift", size);
            break;
        case MINOS_KEY_LEFT_ALT:
            max_strcpy(buf, "Left Alt", size);
            break;
        case MINOS_KEY_CAPSLOCK:
            max_strcpy(buf, "Capslock", size);
            break;
        case MINOS_KEY_SCROLLLOCK:
            max_strcpy(buf, "Scrolllock", size);
            break;
        default:
            if(key < 256) {
               if(size > 2) {
                  buf[0] = key;
                  buf[1] = 0;
               }
               return buf;
            } else {
               if(size) *buf = 0;
            }
    }
    return buf;
}

void ps2_keyboard_handler() {
    size_t i = 0;
    for(;i<PS2_MAX_RETRIES; ++i) {
       uint8_t stat = inb(0x64);
       if(stat & 0x01) break;
    }
    if(i == PS2_MAX_RETRIES) {
        pic_end(1);
        return;
    }
    uint8_t code = inb(0x60);
    switch(code) {
    default: {
         bool released = code >> 7;
         (void)released;
         uint8_t norm = code & ~(1<<7);
         uint16_t key  = norm < ARRAY_LEN(SCAN1_PS2) ? SCAN1_PS2[norm] : 0;
         if(key) {
            ps2queue_push(&ps2queue, (Key) { key, released });
         }
         // else printf("Keyboard: <Unknown %02X>\n",code);
    }
    }
    pic_end(1);
}

static FsOps ps2keyboardOps = {0};
static intptr_t ps2keyboard_read(VfsFile* file, void* buf, size_t size, off_t offset) {
    (void)offset;
    if(size % sizeof(Key) != 0) return -SIZE_MISMATCH;
    size_t count = size / sizeof(Key);
    Key* keys = buf;
    for(size_t i = 0; i < count; ++i) {
        if(!ps2queue_pop((PS2Queue*)file->private, &keys[i])) return i*sizeof(Key);
    }
    return count * sizeof(Key);
}
static intptr_t ps2keyboard_open(Device* this, VfsFile* file, fmode_t mode) {
    if(mode != MODE_READ) return -UNSUPPORTED;
    file->private = this->private;
    return 0;
}
static intptr_t ps2keyboard_init() {
    memset(&ps2keyboardOps, 0, sizeof(ps2keyboardOps));
    ps2keyboardOps.read = ps2keyboard_read;
    return 0;
}
Device ps2keyboard_device = {
    &ps2keyboardOps,
    &ps2queue,
    ps2keyboard_open,
    ps2keyboard_init,
    NULL,
};
