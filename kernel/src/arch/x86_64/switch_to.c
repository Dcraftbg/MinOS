#include "kernel.h"
#include "apic.h"
#include "log.h"
#include "task.h"
void _switch_to(Task* from, Task* to) {
    kernel.processors[get_lapic_id()].current_task = to;
}
