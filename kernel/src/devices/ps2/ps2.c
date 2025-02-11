#include "ps2.h"
#include "keyboard/keyboard.h"
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
        if(cmd.flags & PS2_CMD_FLAG_MOUSE) outb(PS2_DATA_PORT, 0xD4);
        outb(PS2_DATA_PORT, cmd.cmd);
    }
}
void ps2_handle_ack(void) {
    if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_DATA) {
        outb(PS2_DATA_PORT, ps2_cmd_queue.addr[ps2_cmd_queue.tail].data);
        ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags &= ~PS2_CMD_FLAG_DATA;
        return; 
    }
    ps2_cmd_queue.tail = (ps2_cmd_queue.tail + 1) & ps2_cmd_queue.mask;
    if(ps2_cmd_queue.tail != ps2_cmd_queue.head) {
        if(ps2_cmd_queue.addr[ps2_cmd_queue.tail].flags & PS2_CMD_FLAG_MOUSE) outb(PS2_DATA_PORT, 0xD4);
        outb(PS2_DATA_PORT, ps2_cmd_queue.addr[ps2_cmd_queue.tail].cmd);
    }
}
void init_ps2() {
    init_ps2_keyboard();
    intptr_t e = 0;
    if((e = vfs_register_device("ps2keyboard", &ps2keyboard_device)) < 0) {
        kerror("Could not register ps2keyboard device: %s", status_str(e));
    }
}
