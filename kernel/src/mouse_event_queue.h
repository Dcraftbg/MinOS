#pragma once
#include <minos/mouse.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef struct {
    MouseEvent* addr;
    size_t mask;
    size_t head;
    size_t tail;
} MouseEventQueue;
void mouse_event_queue_push(MouseEventQueue* queue, const MouseEvent event);
bool mouse_event_queue_pop(MouseEventQueue* queue, MouseEvent* event);
static MouseEventQueue mouse_event_queue_create(void* addr, size_t mask) {
    return (MouseEventQueue){ addr, mask, 0, 0 };
}
