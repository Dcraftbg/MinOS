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
#include "page.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/enable_arch_extra.h"
#include "arch/x86_64/exception.h"
#include "vfs.h"
#include "rootfs.h"
#include "mem/slab.h"
#include "string.h"
#include "process.h"
#include "task.h"
#include "task_switch.h"
#include "exec.h" 
#include "pic.h"
#include "devices.h"
#include "./devices/tty/tty.h"
#include <minos/keycodes.h>
#include <minos/key.h>
#include <sync.h>
#include "cmdline.h"
#include "charqueue.h"
#include "filelog.h"
#include "iomem.h"
#include "acpi.h"
#include "apic.h"
#include "pci.h"
#include "interrupt.h"
#include "general_caches.h"
#include "epoll.h"
#include "sockets/minos.h"
#include "smp.h"
#include "mem/shared_mem.h"

#include "term/fb/fb.h"

void spawn_init(void) {
    intptr_t e = 0;
    const char* epath = NULL;
    Args args;
    Args env;
    epath = "/user/init";
    const char* argv[] = {epath, "test_arg"};
    args = create_args(ARRAY_LEN(argv), argv);
    const char* envv[] = {"FOO=BAR", "BAZ=A"};
    env  = create_args(ARRAY_LEN(envv), envv);
    if((e = exec_new(epath, &args, &env)) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }
    kinfo("Spawning `%s` id=%zu tid=%zu", epath, (size_t)e, ((Process*)(kernel.processes.items[(size_t)e]))->main_thread->id);
}
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
    reload_tss();
#endif
    init_bitmap();
    init_paging();
    KERNEL_SWITCH_VTABLE();
#ifndef GLOBAL_STORAGE_GDT_IDT
    init_gdt();
    disable_interrupts();
    init_idt();
    init_exceptions();
    reload_tss();
#endif
    enable_cpu_features();
    // Interrupt controller Initialisation
    init_pic();
    kernel.logger->level = LOG_WARN;
    init_acpi();
    kernel.logger->level = LOG_ALL;
    enable_interrupts();
    // Caches
    init_cache_cache();
    minos_socket_init_cache();
    init_epoll_cache();
    init_general_caches();
    init_charqueue();
    // Devices
    init_pci();
    // SMP
    init_smp();
    // Initialisation for process related things
    init_memregion();
    init_processes();
    init_tasks();
    init_kernel_task();
    init_schedulers();
    init_task_switch();
    init_resources();
    init_shm_cache();
    // VFS
    init_vfs();
    init_rootfs();
    init_devices();
    init_tty();

    spawn_init();
    // If you run into problems with PS2. Enable this:
    // I have no idea why this shit works but I think it tells the controller
    // I'm ready to listen for keyboard input or something when I haven't answered its interrupts before
    // (i.e. it thinks it shouldn't send interrupts and this kind of wakes it up saying, hey dude, I'm listening)
#if 0
    while((inb(0x64) & 0x01) == 0);
    kinfo("Got key code %02X", inb(0x60));
#endif
    disable_interrupts();
    irq_clear(kernel.task_switch_irq);
    enable_interrupts();
    for(;;) {
        asm volatile("hlt");
    }
}
