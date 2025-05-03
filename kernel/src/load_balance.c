#include "load_balance.h"
#include "kernel.h"
// TODO: Smarter load balancer that distributes
// tasks better
size_t pick_processor_for_task(void) {
    size_t id;
    // Kind of a hacky solution but what can ya do
    do {
        id = (kernel.load_balancer_head++) % (kernel.max_processor_id + 1);
    } while(!kernel.processors[id].initialised);
    return id;
}
