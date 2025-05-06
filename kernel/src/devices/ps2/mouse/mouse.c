#include "mouse.h"
#include <mouse_event_queue.h>
#include <stdint.h>
#include <stdbool.h>
#include <interrupt.h>
#include <port.h>
#include <log.h>
#define EVENT_QUEUE_CAP 128 
static MouseEvent event_queue_buf[EVENT_QUEUE_CAP] = { 0 };
static MouseEventQueue event_queue = { 0 };

#define PS2_MOUSE_SET_SAMPLE_RATE 0xF3
#define PS2_MOUSE_ENABLE_PACKET_STREAM 0xF4
static enum {
    PS2_MOUSE_STATE_WAIT_FLAGS,
    PS2_MOUSE_STATE_WAIT_X,
    PS2_MOUSE_STATE_WAIT_Y,
} state = PS2_MOUSE_STATE_WAIT_FLAGS;
static uint8_t packet_flags, packet_x, packet_y;
static uint8_t button_flags;
#include "framebuffer.h"
#include "bootutils.h"
void ps2_mouse_handler() {
    size_t i = 0;
    for(;i<PS2_MAX_RETRIES; ++i) {
       uint8_t stat = inb(PS2_STAT_PORT);
       if(stat & 0x01) break;
    }
    if(i == PS2_MAX_RETRIES) 
        goto end;
    uint8_t code = inb(PS2_DATA_PORT);
    if(code == PS2_ACK && ps2_cmd_queue.tail != ps2_cmd_queue.head) {
        ps2_handle_ack();
        goto end;
    }
    if(code == PS2_RESEND && ps2_cmd_queue.tail != ps2_cmd_queue.head) {
        ps2_handle_resend();
        goto end;
    }
    switch(state) {
    case PS2_MOUSE_STATE_WAIT_FLAGS:
        if(!(code & (1 << 3))) {
            kwarn("ps2_mouse_handler> Package corrupted %02X", code);
            goto end;
        }
        packet_flags = code;
        state++;
        break;
    case PS2_MOUSE_STATE_WAIT_X:
        packet_x = code;
        state++;
        break;
    case PS2_MOUSE_STATE_WAIT_Y: {
        packet_y = code;
        int deltaX = packet_x;
        if(packet_flags & (1 << 4)) deltaX |= 0xFFFFFF00;
        int deltaY = packet_y;
        if(packet_flags & (1 << 5)) deltaY |= 0xFFFFFF00;
        if(deltaX || deltaY) {
            mouse_event_queue_push(&event_queue, (MouseEvent){MOUSE_EVENT_KIND_MOVE, .as = { .move = { deltaX, -deltaY } }});
        }
        for(size_t i = 0; i < 3; ++i) {
            if((packet_flags & (1 << i)) != (button_flags & (1 << i))) {
                uint32_t button = 0;
                if(packet_flags & (1 << i)) button |= MOUSE_BUTTON_ON_MASK;
                switch(i) {
                case 0:
                    button |= MOUSE_BUTTON_CODE_LEFT;
                    break;
                case 1:
                    button |= MOUSE_BUTTON_CODE_RIGHT;
                    break;
                case 2:
                    button |= MOUSE_BUTTON_CODE_MIDDLE;
                    break;
                }
                mouse_event_queue_push(&event_queue, (MouseEvent){MOUSE_EVENT_KIND_BUTTON, .as = { .button = button }});
            }
        }
        button_flags = packet_flags & 0x07;
        state = PS2_MOUSE_STATE_WAIT_FLAGS;
    } break;
    }
end:
    irq_eoi(12);
}

static intptr_t ps2mouse_read(Inode* file, void* buf, size_t size, off_t offset) {
    (void)offset;
    if(size % sizeof(MouseEvent) != 0) return -SIZE_MISMATCH;
    size_t count = size / sizeof(MouseEvent);
    MouseEvent* events = buf;
    for(size_t i = 0; i < count; ++i) {
        if(!mouse_event_queue_pop((MouseEventQueue*)file->priv, &events[i])) return i*sizeof(MouseEvent);
    }
    return count * sizeof(MouseEvent);
}
static bool ps2mouse_is_readable(Inode* file) {
    MouseEventQueue* queue = (MouseEventQueue*)file->priv;
    return queue->head != queue->tail;
}
static InodeOps inodeOps = {
    .read = ps2mouse_read,
    .is_readable = ps2mouse_is_readable,
};
intptr_t init_ps2_mouse(void) {
    intptr_t e;
    ps2_mouse_device = new_inode();
    if(ps2_mouse_device) {
        ps2_mouse_device->ops = &inodeOps;
        ps2_mouse_device->priv = &event_queue;
    }
    static_assert(EVENT_QUEUE_CAP > 0 && is_power_of_two(EVENT_QUEUE_CAP), "EVENT_QUEUE_CAP must be a power of 2");
    event_queue = mouse_event_queue_create(event_queue_buf, EVENT_QUEUE_CAP-1);
    if((e=ps2_cmd_controller(PS2_CMD_ENABLE_PORT2)) < 0) return e;
    if((e=ps2_cmd_controller(PS2_CMD_GET_CONFIG)) < 0) return e;
    if((e=ps2_read_u8()) < 0) return e;
    uint8_t config = e;
    ktrace("ps2> Config %02X", config);
    if(!(config & (1 << 5))) {
        if((e=ps2_cmd_controller(0xA7)) < 0) return e;
        config |=  ((1 << 1) | (1 << 5));
        if((e=ps2_cmd_controller2(PS2_CMD_SET_CONFIG, config)) < 0) return e;
    #if 0
        if((e=ps2_cmd_controller(PS2_CMD_GET_CONFIG)) < 0) return e;
        if((e=ps2_read_u8()) < 0) return e;
        config = e;
        kinfo("Config %02X", config);
    #endif
        assert(irq_register(12, idt_ps2_mouse_handler, 0) >= 0);
        irq_clear(12);
        if((e=ps2_cmd_controller(PS2_CMD_ENABLE_PORT2)) < 0) return e;
        ps2_cmd_queue_issue(&ps2_cmd_queue, (PS2Cmd){PS2_CMD_FLAG_MOUSE, 0, PS2_MOUSE_ENABLE_PACKET_STREAM, 0});
    }
    return 0;
}
Inode* ps2_mouse_device = NULL;
