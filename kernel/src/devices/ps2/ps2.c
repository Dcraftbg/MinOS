#include "ps2.h"
#include <devices/multiplexer.h>
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"
#include <interrupt.h>
#include <log.h>

#define PS2_CMD_QUEUE_CAP 8
static_assert(PS2_CMD_QUEUE_CAP > 0 && is_power_of_two(PS2_CMD_QUEUE_CAP), "PS2_CMD_QUEUE_CAP must be a power of two!");
#define PS2_CMD_QUEUE_MASK (PS2_CMD_QUEUE_CAP - 1)

static PS2Cmd ps2_cmd_queue_buf[PS2_CMD_QUEUE_CAP];
PS2CmdQueue ps2_cmd_queue = { 
    .addr = ps2_cmd_queue_buf,
    .head = 0,
    .tail = 0,
    .mask = PS2_CMD_QUEUE_MASK
};
void ps2_cmd_queue_issue(PS2CmdQueue* queue, const PS2Cmd cmd) {
    debug_assert(queue->addr);
    queue->head &= queue->mask;
    if(((queue->head + 1) & queue->mask) == queue->tail) {
        kwarn("PS2 queue overflow!");
        return;
    }
    queue->addr[queue->head] = cmd;
    if((queue->head++) == queue->tail) {
        queue->addr[queue->tail].state = PS2_CMD_STATE_WAIT_ACK;
        if(cmd.flags & PS2_CMD_FLAG_MOUSE) outb(PS2_STAT_PORT, 0xD4);
        outb(PS2_DATA_PORT, cmd.cmd);
    }
}
void ps2_handle_resend(void) {
    switch(ps2_cmd_queue.addr[ps2_cmd_queue.tail].state) {
    case PS2_CMD_STATE_WAIT_ACK:
        if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_MOUSE) outb(PS2_STAT_PORT, 0xD4);
        outb(PS2_DATA_PORT, ps2_cmd_queue.addr[ps2_cmd_queue.tail].cmd);
        break;
    case PS2_CMD_STATE_WAIT_ACK_DATA:
        if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_MOUSE) outb(PS2_STAT_PORT, 0xD4);
        outb(PS2_DATA_PORT, ps2_cmd_queue.addr[ps2_cmd_queue.tail].data);
        break;
    default:
        kerror("ps2_handle_resend> other %02X cmd=%02X", ps2_cmd_queue.addr[ps2_cmd_queue.tail].state, ps2_cmd_queue.addr[ps2_cmd_queue.tail].cmd);
    }
}
void ps2_handle_ack(void) {
    switch(ps2_cmd_queue.addr[ps2_cmd_queue.tail].state) {
    case PS2_CMD_STATE_WAIT_ACK:
        if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_DATA) {
            ps2_cmd_queue.addr[ps2_cmd_queue.tail].state = PS2_CMD_STATE_WAIT_ACK_DATA;
            if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_MOUSE) outb(PS2_STAT_PORT, 0xD4);
            outb(PS2_DATA_PORT, ps2_cmd_queue.addr[ps2_cmd_queue.tail].data);
            return; 
        }
        ps2_cmd_queue.tail = (ps2_cmd_queue.tail + 1) & ps2_cmd_queue.mask;
        return;
    case PS2_CMD_STATE_WAIT_ACK_DATA:
        ps2_cmd_queue.tail = (ps2_cmd_queue.tail + 1) & ps2_cmd_queue.mask;
        if(ps2_cmd_queue.tail != ps2_cmd_queue.head) {
            ps2_cmd_queue.addr[ps2_cmd_queue.tail].state = PS2_CMD_STATE_WAIT_ACK;
            if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_MOUSE) outb(PS2_STAT_PORT, 0xD4);
            outb(PS2_DATA_PORT, ps2_cmd_queue.addr[ps2_cmd_queue.tail].cmd);
        }
    default:
        kerror("ps2_handle_ack> other %02X", ps2_cmd_queue.addr[ps2_cmd_queue.tail].state);
    }
}
intptr_t ps2_wait_write(void) {
    size_t tries = 100000;
    while((inb(PS2_STAT_PORT) & 0b10) != 0 && (--tries));
    return tries == 0 ? -TIMEOUT_REACHED : 0;
}
intptr_t ps2_wait_read(void) {
    size_t tries = 100000;
    while((inb(PS2_STAT_PORT) & 0b1) == 0 && (--tries));
    return tries == 0 ? -TIMEOUT_REACHED : 0;
}
intptr_t ps2_cmd_controller(uint8_t cmd) {
    intptr_t e;
    if((e=ps2_wait_write()) < 0) return e;
    outb(0x64, cmd);
    return 0;
}
intptr_t ps2_cmd_controller2(uint8_t cmd, uint8_t data) {
    intptr_t e;
    if((e=ps2_cmd_controller(cmd)) < 0) return e;
    if((e=ps2_wait_write()) < 0) return e;
    outb(PS2_DATA_PORT, data);
    return 0;
}
intptr_t ps2_read_u8(void) {
    intptr_t e;
    if((e=ps2_wait_read()) < 0) return e;
    return inb(PS2_DATA_PORT);
}
intptr_t ps2_cmd_send(uint8_t prefix, uint8_t cmd) {
    intptr_t e;
    if(prefix) {
        if((e=ps2_wait_write()) < 0) return e;
        outb(PS2_STAT_PORT, prefix);
    }
    if((e=ps2_wait_write()) < 0) return e;
    outb(PS2_DATA_PORT, cmd);
    return 0;
}
void init_ps2() {
    intptr_t e = 0;
    while(ps2_read_u8() >= 0);
    if((e = init_ps2_mouse()) < 0) {
        kerror("Failed to initialise PS2 mouse: %s", status_str(e));
    }
    init_ps2_keyboard();
    if((e = vfs_register_device("ps2keyboard", &ps2keyboard_device)) < 0) {
        kerror("Could not register ps2keyboard device: %s", status_str(e));
    }
    Inode* keyboard_inode;
    if((e= vfs_find_abs("/devices/ps2keyboard", &keyboard_inode)) < 0) {
        kfatal("Registered ps2keyboard device successfully but failed to get it?");
        return;
    }
    multiplexer_add(&keyboard_mp, keyboard_inode);
    if((e = vfs_register_device("ps2mouse", &ps2mouse_device)) < 0) {
        kerror("Could not register ps2mouse device: %s", status_str(e));
    }
}
