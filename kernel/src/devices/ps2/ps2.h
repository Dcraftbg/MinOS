#pragma once
#include <stdint.h>
#include <stddef.h>
#define PS2_MAX_RETRIES 3
#define PS2_CMD_FLAG_MOUSE     0b1
#define PS2_CMD_FLAG_DATA      0b10
#define PS2_DATA_PORT 0x60
#define PS2_STAT_PORT 0x64

#define PS2_ACK 0xFA
typedef struct {
    uint8_t flags;
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
void init_ps2();
