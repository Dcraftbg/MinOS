#include "task.h"
#include "apic.h"
#include "interrupt.h"
#include "switch_to.h"
#include "kernel.h"
#include "log.h"
typedef struct TaskRegs TaskRegs;
void task_switch(TaskRegs* regs) {
    Task* current = current_task();
    Processor* processor = &kernel.processors[get_lapic_id()];
    Task* select = task_select(&processor->scheduler);
    processor->lapic_ticks++;
    if(select && select != current) {
        irq_eoi(kernel.task_switch_irq);
        switch_to(current, select);
    }
    irq_eoi(kernel.task_switch_irq);
}
void init_task_switch(void) {
    irq_register(kernel.task_switch_irq, task_switch, 0);
}
