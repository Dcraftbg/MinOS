// TODO: Refactor this out? Idk how necessary it is. I feel like this is just way simpler.
// Then again a preboot will just fix all of this anyway so I'm postponing :)
#include <limine.h>
#include "log.h"
static volatile struct limine_smp_request limine_smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .flags = 0,
};
extern void ap_init(struct limine_smp_info*);
#define AP_STACK_SIZE 1*PAGE_SIZE
#include "arch/x86_64/gdt.h"

static Mutex tss_sync = { 0 };
void ap_main(struct limine_smp_info* info) {
    reload_idt();
    reload_gdt();
    kernel_reload_gdt_registers();
    mutex_lock(&tss_sync);
    tss_load_cpu();
    mutex_unlock(&tss_sync);
    kinfo("Hello from logical processor %zu", info->lapic_id);
}
void init_smp(void) {
    if(!limine_smp_request.response) return;
    kinfo("There are %zu cpus", limine_smp_request.response->cpu_count);
    for(size_t i = 0; i < limine_smp_request.response->cpu_count; ++i) {
        struct limine_smp_info* info = limine_smp_request.response->cpus[i];
        if(info->lapic_id == limine_smp_request.response->bsp_lapic_id) continue; 
        // FIXME: This obviously leaks just like how the kernel stack kinda leaks.
        // I think its fine but leaving this FIXME just in case
        void* ap_stack_base_addr = kernel_malloc(AP_STACK_SIZE);
        assert(ap_stack_base_addr && "Not enough memory for processors stack");
        info->extra_argument = ((uintptr_t)ap_stack_base_addr) + AP_STACK_SIZE;
        info->goto_address = (void*) &ap_init;
    }
}
