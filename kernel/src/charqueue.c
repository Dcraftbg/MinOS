#include "charqueue.h"
#include "assert.h"
#include "log.h"
#include "kernel.h"
void charqueue_push(CharQueue* queue, uint32_t point) {
    debug_assert(queue->addr);
    queue->head &= queue->mask;
    size_t at = queue->head++;
    if((queue->head & queue->mask) == queue->tail) {
        kwarn("charqueue overflow");
    }
    queue->addr[at] = point;
}
bool charqueue_pop(CharQueue* queue, uint32_t* key) {
    if(queue->tail == queue->head) return false;
    queue->tail &= queue->mask;
    *key = queue->addr[queue->tail++];
    return true;
}

CharQueue* charqueue_new(uint32_t* addr, size_t mask) {
    if(!kernel.charqueue_cache) return NULL;
    CharQueue* queue = cache_alloc(kernel.charqueue_cache);
    if(!queue) return NULL;
    charqueue_init(queue, addr, mask);
    return queue;
}

void init_charqueue() {
    if(!(kernel.charqueue_cache=create_new_cache(sizeof(CharQueue), "CharQueue"))) {
        kerror("Failed to allocate CharQueue cache");
    }
}
