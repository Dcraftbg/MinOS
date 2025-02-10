#pragma once
#include <minos/key.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define KEY_MAX_ENTRIES (1<<11)
typedef struct {
    Key* addr;
    size_t mask;
    size_t head;
    size_t tail;
} KeyQueue;
void keyqueue_push(KeyQueue* queue, const Key key);
bool keyqueue_pop(KeyQueue* queue, Key* key);
static KeyQueue keyqueue_create(void* addr, size_t mask) {
    return (KeyQueue){ addr, mask, 0, 0 };
}
