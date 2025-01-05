#include "../../config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "port.h"
#include "serial.h"
#include "logger.h"
#include "log.h"
#include "assert.h"
#include "print.h"
#include "utils.h"
#include "memory.h"
#include "mem/bitmap.h"
#include "kernel.h"
#include "debug.h"
#include "page.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/enable_arch_extra.h"
#include "exception.h"
#include "vfs.h"
#include "rootfs.h"
#include "mem/slab.h"
#include "string.h"
#include "process.h"
#include "task.h"
#include "exec.h" 
#include "pic.h"
#include "pit.h"
#include "syscall.h"
#include "devices.h"
#include "./devices/tty/tty.h"
#include <minos/keycodes.h>
#include <minos/key.h>
#include <sync.h>
#include "heap.h"
#include "cmdline.h"
#include "charqueue.h"
#include "filelog.h"
#include "iomem.h"
#include "acpi.h"
#include "apic.h"
#include "pci.h"
#include "interrupt.h"
#include "general_caches.h"

#include "term/fb/fb.h"
// TODO: create a symlink "/devices/keyboard" which will be a link to the currently selected keyboard
// Like for example PS1 or USB or anything like that
static void fbt() {
    Framebuffer buf = get_framebuffer_by_id(0);
    if(!buf.addr) return;
    if(buf.bpp != 32) return;
    fmbuf_fill(&buf, VGA_BG);
}
#if 0
#include "probe.h"
static void do_probe() {
    wang_seed = limine_boot_time_request.response->boot_time;
    printf("Boot time: %zu\n", (size_t)limine_boot_time_request.response->boot_time);
    probe_slab();
    Framebuffer fb = get_framebuffer_by_id(0);
    fmbuf_fill(&fb, 0x00ff00);
}
#endif

void _start() {
    disable_interrupts();
    BREAKPOINT();
    serial_init();
    kernel.logger = &serial_logger;
    init_cmdline();
    init_loggers();
#ifdef GLOBAL_STORAGE_GDT_IDT
    init_gdt();
    disable_interrupts();
    init_idt();
    init_exceptions();
    tss_load_cpu(0);
#endif
    init_bitmap();
    init_paging();
    KERNEL_SWITCH_VTABLE();
#ifndef GLOBAL_STORAGE_GDT_IDT
    init_gdt();
    disable_interrupts();
    init_idt();
    init_exceptions();
    tss_load_cpu(0);
#endif
    enable_cpu_features();
    // Interrupt controller Initialisation
    init_pic();
    init_acpi();
    enable_interrupts();
    // Caches
    init_cache_cache();
    assert(kernel.device_cache = create_new_cache(sizeof(Device), "Device"));
    init_general_caches();
    init_charqueue();
    // Devices
    init_pci();
    // Initialisation for process related things
    init_memregion();
    init_processes();
    init_tasks();
    init_heap();
    init_kernel_task();
    init_task_switch();
    init_syscalls();
    init_resources();
    fbt();
    // VFS
    init_vfs();
    init_rootfs();
    init_devices();
    init_tty();

    intptr_t e = 0;
    const char* epath = NULL;
    Args args;
    Args env;
    epath = "/user/nothing";
    args = create_args(1, &epath);
    env  = create_args(0, NULL);
    if((e = exec_new(epath, &args, &env)) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }
    kinfo("Spawning `%s` id=%zu", epath, (size_t)e);
    epath = "/user/init";
    const char* argv[] = {epath, "test_arg"};
    args = create_args(ARRAY_LEN(argv), argv);
    const char* envv[] = {"FOO=BAR", "BAZ=A"};
    env  = create_args(ARRAY_LEN(envv), envv);
    if((e = exec_new(epath, &args, &env)) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }
    kinfo("Spawning `%s` id=%zu", epath, (size_t)e);
    disable_interrupts();
    irq_clear(1);
    irq_clear(kernel.task_switch_irq);
    enable_interrupts();
    for(;;) {
        asm volatile("hlt");
    }
}
