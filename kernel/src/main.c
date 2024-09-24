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
#include "gdt.h"
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


void _start() {
    BREAKPOINT();
    serial_init();
    init_loggers();
    init_bitmap();
    init_paging();
    KERNEL_SWITCH_VTABLE();
    init_gdt();
    disable_interrupts();
    init_idt();
    init_pic();
    init_exceptions();
    tss_load_cpu(0);
    enable_interrupts();
    init_cache_cache();
    init_vfs();
    init_rootfs();
    init_devices();

    init_memregion();
    init_processes();
    init_tasks();
    init_kernel_task();
    init_task_switch();
    init_syscalls();
    init_resources();
    pit_set_count(1000);
    assert(kernel.device_cache = create_new_cache(sizeof(Device), "Device"));
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
