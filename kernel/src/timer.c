#include "timer.h"
#include "kernel.h"
#include "apic.h"
size_t system_timer_milis(void) {
    return kernel.processors[get_lapic_id()].lapic_ticks;
}
