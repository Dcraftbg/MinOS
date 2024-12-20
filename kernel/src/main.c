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
#include "prog_bar.h"
#include "heap.h"
#include "cmdline.h"
#include "charqueue.h"
#include "filelog.h"
#include "iomem.h"
#include "acpi.h"
// TODO: create a symlink "/devices/keyboard" which will be a link to the currently selected keyboard
// Like for example PS1 or USB or anything like that
static void fbt() {
    Framebuffer buf = get_framebuffer_by_id(0);
    if(!buf.addr) return;
    if(buf.bpp != 32) return;
    fmbuf_fill(&buf, 0xFF212121);
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

    update_bar(step++, "serial_init");
    serial_init();
    kernel.logger = &serial_logger;
    update_bar(step++, "init_cmdline");
    init_cmdline();

    update_bar(step++, "init_loggers");
    init_loggers();

#ifdef GLOBAL_STORAGE_GDT_IDT
    update_bar(step++, "init_gdt");
    init_gdt();
    update_bar(step++, "init_idt");
    init_idt();

    update_bar(step++, "init_exceptions");
    init_exceptions();

    update_bar(step++, "init_tss");
    tss_load_cpu(0);

    update_bar(step++, "init_pic");
    init_pic();

    enable_interrupts();
#endif

    update_bar(step++, "init_bitmap");
    init_bitmap();

    update_bar(step++, "init_paging");
    init_paging();
    KERNEL_SWITCH_VTABLE();

#ifndef GLOBAL_STORAGE_GDT_IDT
    update_bar(step++, "init_gdt");
    init_gdt();
    disable_interrupts();
    update_bar(step++, "init_idt");
    init_idt();

    update_bar(step++, "init_pic");
    init_pic();

    update_bar(step++, "init_exceptions");
    init_exceptions();

    update_bar(step++, "init_tss");
    tss_load_cpu(0);
    enable_interrupts();
#endif
    enable_cpu_features();

    update_bar(step++, "init_cache_cache");
    init_cache_cache();

    update_bar(step++, "init_vfs");
    init_vfs();
    update_bar(step++, "init_rootfs");
    init_rootfs();

    update_bar(step++, "device cache");
    assert(kernel.device_cache = create_new_cache(sizeof(Device), "Device"));

    update_bar(step++, "init_charqueue");
    init_charqueue();

    update_bar(step++, "init_devices");
    init_devices();

    update_bar(step++, "init_memregion");
    init_memregion();

    update_bar(step++, "init_processes");
    init_processes();

    update_bar(step++, "init_tasks");
    init_tasks();

    update_bar(step++, "init_heap");
    init_heap();

    update_bar(step++, "init_kernel_task");
    init_kernel_task();

    update_bar(step++, "init_task_switch");
    init_task_switch();

    update_bar(step++, "init_syscalls");
    init_syscalls();

    update_bar(step++, "init_resources");
    init_resources();

    update_bar(step++, "pit_set_count");
    pit_set_count(1000);

    init_acpi();

    update_bar(step++, "fbt test");
    fbt();
    init_tty();

    // dev_test();
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
    pic_clear_mask(1);
    pic_clear_mask(0);
    for(;;) {
        asm volatile("hlt");
    }
}
