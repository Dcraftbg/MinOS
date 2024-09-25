#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../utils.h"
#include "../../vfs.h"
#include "../../string.h"
#include <minos/key.h>
#include "../../print.h"
#include "../../assert.h"
#include "../../pic.h"
#include "../../memory.h"
#include "../../arch/x86_64/idt.h"
#include "../../kernel.h"

#define PS2_MAX_RETRIES 3
#define PS2_MAX_ENTRIES (1<<11)

typedef struct {
    Key* addr;
    size_t mask;
    size_t head;
    size_t tail;
} PS2Queue;
extern PS2Queue ps2queue;

static void ps2queue_push(PS2Queue* queue, Key key) {
    debug_assert(queue->addr);
    queue->head &= queue->mask;
    size_t at = queue->head++;
    if((queue->head & queue->mask) == queue->tail) {
        printf("WARN: PS2 queue overflow!\n");
    }
    queue->addr[at] = key;
}
static bool ps2queue_pop(PS2Queue* queue, Key* key) {
    if(queue->tail == queue->head) return false;
    queue->tail &= queue->mask;
    size_t at = queue->tail++;
    *key = queue->addr[at];
    return true;
}
static PS2Queue ps2queue_construct(void* addr, size_t mask) {
    return (PS2Queue) { addr, mask, 0, 0 };
}
