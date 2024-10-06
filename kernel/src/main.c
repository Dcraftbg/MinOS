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
// TODO: create a symlink "/devices/keyboard" which will be a link to the currently selected keyboard
// Like for example PS1 or USB or anything like that
static void fbt() {
    Framebuffer buf = get_framebuffer_by_id(0);
    if(!buf.addr) return;
    if(buf.bpp != 32) return;
    fmbuf_fill(&buf, 0xFF212121);
}

static void dev_test() {
    const char* path = "/devices/tty0";
    const char* msg = "This is a test message to test if devices work :D\nHello!";
    intptr_t e = 0;
    VfsFile file={0};
    if((e = vfs_open(path, &file, MODE_WRITE)) < 0) {
        printf("ERROR: Failed to open %s: %s\n",path,status_str(e));
        kabort();
    }
    if((e = write_exact(&file, msg, strlen(msg))) < 0) {
        vfs_close(&file);
        printf("ERROR: Could not write test dev message : %s\n",status_str(e));
        kabort();
    }
    vfs_close(&file);
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

#include "fbwriter.h"
#define TOTAL_STEPS 23 
void update_bar(size_t at, const char* msg) {
    Framebuffer fb = get_framebuffer_by_id(0);
    if(fb.addr) {
        size_t height = 10;
        size_t width = (fb.width-30) * at / TOTAL_STEPS;
        size_t x = 15;
        size_t y = (fb.height-height-5); // (fb.height-height) / 2;
        fmbuf_draw_rect(&fb, x, y, x+width, y+height, 0x00FF00);
        size_t texoff = 20;
        fmbuf_draw_rect(&fb, x, y-texoff, x+fb.width-15, y, 0);
        FbTextWriter writer = {
            .fb = fb,
            .x = x,
            .y = y-texoff,
        };
        fbwriter_draw_cstr(&writer, msg);
    }
}
size_t step=0;
void _start() {

    disable_interrupts();
    BREAKPOINT();

    FbTextWriter writer = {
        .fb = get_framebuffer_by_id(0),
        .x = 0,
        .y = 0,
    };
    fbwriter_draw_cstr(
        &writer,
        "IDT & GDT: " 
#ifdef GLOBAL_STORAGE_GDT_IDT
        "Global Storage"
#else 
        "Heap"
#endif
    );
    update_bar(step++, "serial_init");
    serial_init();

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

    update_bar(step++, "init_cache_cache");
    init_cache_cache();

    update_bar(step++, "init_vfs");
    init_vfs();

    update_bar(step++, "init_rootfs");
    init_rootfs();

    update_bar(step++, "init_devices");
    init_devices();

    update_bar(step++, "init_memregion");
    init_memregion();

    update_bar(step++, "init_processes");
    init_processes();

    update_bar(step++, "init_tasks");
    init_tasks();

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

    update_bar(step++, "device cache");
    assert(kernel.device_cache = create_new_cache(sizeof(Device), "Device"));

    update_bar(step++, "fbt test");
    fbt();
    init_tty();
    // dev_test();
    intptr_t e = 0;
    const char* epath = NULL;
    epath = "/user/nothing";
    if((e = exec_new(epath, create_args(1, &epath))) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }

    epath = "/user/init";
    const char* args[] = {epath, "test_arg"};
    if((e = exec_new(epath, create_args(ARRAY_LEN(args), args))) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }
    pic_clear_mask(1);
    pic_clear_mask(0);
    for(;;) {
        asm volatile("hlt");
    }
}
