#include "keyqueue.h"
#include <assert.h>
#include <log.h>
void keyqueue_push(KeyQueue* queue, const Key key) {
    debug_assert(queue->addr);
    queue->head &= queue->mask;
    size_t at = queue->head++;
    if((queue->head & queue->mask) == queue->tail) {
        kwarn("Key queue overflow!");
    }
    queue->addr[at] = key;
}
bool keyqueue_pop(KeyQueue* queue, Key* key) {
    if(queue->tail == queue->head) return false;
    queue->tail &= queue->mask;
    size_t at = queue->tail++;
    *key = queue->addr[at];
    return true;
}
