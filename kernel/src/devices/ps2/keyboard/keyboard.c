#include "keyboard.h"
#include <stdint.h>
#include <stdbool.h>
#include <keyqueue.h>
#include <interrupt.h>
#include <port.h>
#include <log.h>

static uint16_t SCAN1_PS2[] = {
    0   ,MINOS_KEY_ESCAPE,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,'0' ,'-' ,'=' ,MINOS_KEY_BACKSPACE, MINOS_KEY_TAB,
    'Q' ,'W' ,'E' ,'R' ,'T' ,'Y' ,'U' ,'I' ,'O' ,'P' ,'[' ,']' , MINOS_KEY_ENTER, MINOS_KEY_LEFT_CTRL, 'A' ,'S' ,
    'D' ,'F' ,'G' ,'H' ,'J' ,'K' ,'L' ,';' ,'\'','`' ,MINOS_KEY_LEFT_SHIFT ,'\\' ,'Z' ,'X' ,'C' ,'V' ,
    'B' ,'N' ,'M' ,',' ,'.' ,'/' ,MINOS_KEY_RIGHT_SHIFT ,MINOS_KEY_NUMPAD_STAR ,MINOS_KEY_LEFT_ALT ,' ' ,MINOS_KEY_CAPSLOCK ,MINOS_KEY_F1 ,MINOS_KEY_F2 ,MINOS_KEY_F3 ,MINOS_KEY_F4 ,MINOS_KEY_F5 ,
    MINOS_KEY_F6 ,MINOS_KEY_F7 ,MINOS_KEY_F8 ,MINOS_KEY_F9 ,MINOS_KEY_F10 ,MINOS_KEY_NUMLOCK ,MINOS_KEY_SCROLLLOCK ,MINOS_KEY_NUMPAD_7 ,MINOS_KEY_NUMPAD_8 ,MINOS_KEY_NUMPAD_9 ,MINOS_KEY_NUMPAD_MINUS ,MINOS_KEY_NUMPAD_4 ,MINOS_KEY_NUMPAD_5 ,MINOS_KEY_NUMPAD_6 ,MINOS_KEY_NUMPAD_PLUS ,MINOS_KEY_NUMPAD_1 ,
    MINOS_KEY_NUMPAD_2 ,MINOS_KEY_NUMPAD_3 ,MINOS_KEY_NUMPAD_0 ,MINOS_KEY_NUMPAD_DOT ,0   ,0   ,0   ,MINOS_KEY_F11 ,MINOS_KEY_F12 ,0   ,0   ,0   ,0   ,0   ,0   ,0   ,
};
static uint16_t SCAN1_PS2_EX[128] = {
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    MINOS_KEY_UP_ARROW,    0,                     0,                     MINOS_KEY_LEFT_ARROW,  0,                     MINOS_KEY_RIGHT_ARROW, 0,                     0,                     
    MINOS_KEY_DOWN_ARROW,  0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
    0,                     0,                     0,                     0,                     0,                     0,                     0,                     0,                     
};
#if 0
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
#endif

static bool extended;
static KeyQueue keyqueue;
void ps2_keyboard_handler() {
    size_t i = 0;
    for(;i<PS2_MAX_RETRIES; ++i) {
       uint8_t stat = inb(0x64);
       if(stat & 0x01) break;
    }
    if(i == PS2_MAX_RETRIES) 
        goto end;
    uint8_t code = inb(0x60);
    switch(code) {
    case PS2_ACK:
        ps2_handle_ack();
        break;
    case PS2_RESEND:
        ps2_handle_resend();
        break;
    case 0xE0:
        extended = true;
        break;
    default: {
        bool released = code >> 7;
        (void)released;
        uint8_t norm  = code & ~(1<<7);
        uint16_t key  = 
            extended ? 
                norm < ARRAY_LEN(SCAN1_PS2_EX) ? SCAN1_PS2_EX[norm] : 0 :
                norm < ARRAY_LEN(SCAN1_PS2   ) ? SCAN1_PS2   [norm] : 0;
        if(key) {
           keyqueue_push(&keyqueue, (Key) { key, released });
        }
        extended = false;
        // else printf("Keyboard: <Unknown %02X>\n",code);
    }
    }
end:
    irq_eoi(1);
}
static bool ps2keyboard_is_readable(Inode* file) {
    return ((KeyQueue*)file->priv)->head != ((KeyQueue*)file->priv)->tail;
}
static intptr_t ps2keyboard_read(Inode* file, void* buf, size_t size, off_t offset) {
    (void)offset;
    if(size % sizeof(Key) != 0) return -SIZE_MISMATCH;
    size_t count = size / sizeof(Key);
    Key* keys = buf;
    for(size_t i = 0; i < count; ++i) {
        if(!keyqueue_pop((KeyQueue*)file->priv, &keys[i])) return i*sizeof(Key);
    }
    return count * sizeof(Key);
}
static InodeOps inodeOps = {
    .read = ps2keyboard_read,
    .is_readable = ps2keyboard_is_readable
};
Inode* ps2_keyboard_device = NULL;
#define KEYQUEUE_CAP 4096
void init_ps2_keyboard() {
    static_assert(KEYQUEUE_CAP > 0 && is_power_of_two(KEYQUEUE_CAP), "KEYQUEUE_CAP must be a power of two!");
    keyqueue = keyqueue_create(kernel_malloc(KEYQUEUE_CAP), KEYQUEUE_CAP-1);
    if(!keyqueue.addr) {
        kwarn("Failed to allocate ps2 key queue!");
        return;
    }
    ps2_keyboard_device = new_inode();
    if(ps2_keyboard_device) {
        ps2_keyboard_device->ops = &inodeOps;
        ps2_keyboard_device->priv = &keyqueue;
    }
    assert(irq_register(1, idt_ps2_keyboard_handler, 0) >= 0);
    irq_clear(1);
}
