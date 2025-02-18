#include "mouse_event_queue.h"
#include <assert.h>
#include <log.h>
void mouse_event_queue_push(MouseEventQueue* queue, const MouseEvent key) {
    debug_assert(queue->addr);
    queue->head &= queue->mask;
    size_t at = queue->head++;
    if((queue->head & queue->mask) == queue->tail) {
        kwarn("Mouse Event queue overflow!");
    }
    queue->addr[at] = key;
}
bool mouse_event_queue_pop(MouseEventQueue* queue, MouseEvent* key) {
    if(queue->tail == queue->head) return false;
    queue->tail &= queue->mask;
    size_t at = queue->tail++;
    *key = queue->addr[at];
    return true;
}
