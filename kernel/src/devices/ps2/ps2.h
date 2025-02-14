#pragma once
#include <stdint.h>
#include <stddef.h>
#define PS2_MAX_RETRIES 3
#define PS2_CMD_FLAG_MOUSE 0b1
#define PS2_CMD_FLAG_DATA  0b10
#define PS2_DATA_PORT 0x60
#define PS2_STAT_PORT 0x64

#define PS2_RESEND 0xFE
#define PS2_ACK    0xFA

#define PS2_CMD_ENABLE_PORT2 0xA8
#define PS2_CMD_GET_CONFIG 0x20
#define PS2_CMD_SET_CONFIG 0x60
enum {
    PS2_CMD_STATE_NONE,
    PS2_CMD_STATE_WAIT_ACK,
    PS2_CMD_STATE_WAIT_ACK_DATA
};
typedef struct {
    uint8_t flags;
    uint8_t state;
    uint8_t cmd;
    uint8_t data;
} PS2Cmd;
typedef struct {
    PS2Cmd* addr;
    uint8_t head, tail;
    uint8_t mask;
} PS2CmdQueue;
extern PS2CmdQueue ps2_cmd_queue;
void ps2_cmd_queue_issue(PS2CmdQueue* queue, const PS2Cmd cmd);
void ps2_handle_ack(void);
void ps2_handle_resend(void);

intptr_t ps2_wait_write(void);
intptr_t ps2_wait_read(void);
intptr_t ps2_cmd_controller(uint8_t cmd);
intptr_t ps2_cmd_controller2(uint8_t cmd, uint8_t data);
intptr_t ps2_cmd_send(uint8_t prefix, uint8_t cmd);
intptr_t ps2_read_u8(void);
void init_ps2();
