#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    uint32_t* addr;
    size_t mask;
    size_t head;
    size_t tail;
} CharQueue;
void charqueue_push(CharQueue* queue, uint32_t point);
bool charqueue_pop(CharQueue* queue, uint32_t* key);
static void charqueue_init(CharQueue* queue, uint32_t* addr, size_t mask) {
    queue->addr = addr;
    queue->mask = mask;
    queue->head = 0;
    queue->tail = 0;
}
void init_charqueue();
CharQueue* charqueue_new(uint32_t* addr, size_t mask);
