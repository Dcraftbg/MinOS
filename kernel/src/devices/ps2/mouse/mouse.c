#include "mouse.h"
#include <stdint.h>
#include <stdbool.h>
#include <interrupt.h>
#include <port.h>
#include <log.h>

#define PS2_MOUSE_SET_SAMPLE_RATE 0xF3
#define PS2_MOUSE_ENABLE_PACKET_STREAM 0xF4
static enum {
    PS2_MOUSE_STATE_WAIT_FLAGS,
    PS2_MOUSE_STATE_WAIT_X,
    PS2_MOUSE_STATE_WAIT_Y,
} state = PS2_MOUSE_STATE_WAIT_FLAGS;
static uint8_t packet_flags, packet_x, packet_y;
static int x = 0, y = 0;
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
#if 0
        ktrace("ps2_mouse_handler> Full packet (flags=%02X, x=%02X, y=%02X)", packet_flags, packet_x, packet_y);
#endif
        int deltaX = packet_x;
        if(packet_flags & (1 << 4)) deltaX |= 0xFFFFFF00;
        int deltaY = packet_y;
        if(packet_flags & (1 << 5)) deltaY |= 0xFFFFFF00;
#if 0
        Framebuffer buf = get_framebuffer_by_id(0);
        if(!buf.addr) return;
        if(buf.bpp != 32) return;
        if(!(packet_flags & (1 << 0))) {
            fmbuf_draw_rect(&buf, x, y, x + 10, y + 10, 0xff212121);
        }
#endif
        x += deltaX;
        y -= deltaY;
        state = PS2_MOUSE_STATE_WAIT_FLAGS;
#if 0
        if(x < 0) x = 0;
        if(y < 0) y = 0;
        if(x > buf.width - 10) x = buf.width - 10;
        if(y > buf.height - 10) y = buf.height - 10;
        fmbuf_draw_rect(&buf, x, y, x + 10, y + 10, 0xFFFF0000);
#endif
    } break;
    }
end:
    irq_eoi(12);
}

static InodeOps inodeOps = {
};
static intptr_t init_inode(Device* this, Inode* inode) {
    inode->priv = this->priv;
    inode->ops = &inodeOps;
    return 0;
}
intptr_t init_ps2_mouse(void) {
    intptr_t e;
    if((e=ps2_cmd_controller(PS2_CMD_ENABLE_PORT2)) < 0) return e;
    if((e=ps2_cmd_controller(PS2_CMD_GET_CONFIG)) < 0) return e;
    if((e=ps2_read_u8()) < 0) return e;
    uint8_t config = e;
    ktrace("ps2> Config %02X", config);
    config |= 1 << 1;
    if((e=ps2_cmd_controller2(PS2_CMD_SET_CONFIG, config)) < 0) return e;
#if 0
    if((e=ps2_cmd_controller(PS2_CMD_GET_CONFIG)) < 0) return e;
    if((e=ps2_read_u8()) < 0) return e;
    config = e;
    kinfo("Config %02X", config);
#endif
    assert(irq_register(12, idt_ps2_mouse_handler, 0) >= 0);
    irq_clear(12);
    ps2_cmd_queue_issue(&ps2_cmd_queue, (PS2Cmd){PS2_CMD_FLAG_MOUSE, 0, PS2_MOUSE_ENABLE_PACKET_STREAM, 0});
    return 0;
}
Device ps2mouse_device = {
    .init_inode=init_inode,
};
