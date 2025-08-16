#include "tss.h"
#include "kernel.h"
void reload_tss(void) {
    TSSSegment* tss = (TSSSegment*)&kernel.gdt.tss;
    tss->limit_low = sizeof(TSS)-1;
    uint64_t tss_ptr = (uint64_t)&kernel.tss;
    tss->base_low    = tss_ptr;
    tss->base_middle = tss_ptr>>16;
    tss->base_high   = tss_ptr>>24;
    tss->base_high2  = tss_ptr>>32;
    tss->access = 0x89;
    tss->_reserved = 0;
    tss->limit_flags = 0x20;
    __asm__ volatile(
        "ltr %0"
        :
        : "r" ((uint16_t)GDT_TSS) // Offset within the GDT
    );
    kernel_reload_gdt_registers();
}
