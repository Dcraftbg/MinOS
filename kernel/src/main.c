#include "../../config.h"
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "port.h"
#include "serial.h"
#include "assert.h"
#include "print.h"
#include "utils.h"
#include "memory.h"
#include "mmap.h"
#include "kernel.h"
#include "debug.h"
#include "page.h"
#include "gdt.h"
#include "exception.h"
#include "vfs.h"
#include "slab.h"
#include "string.h"
#include "task.h"
#include "exec.h" 
#include "pic.h"
#include "pit.h"
#include "syscall.h"
#include "./devices/serial/serial.h"
#include "./devices/vga/vga.h"
#include "vga.h"

volatile struct limine_framebuffer_request limine_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};
static void fbt() {
    Framebuffer buf = get_framebuffer_by_id(0);
    if(!buf.addr) return;
    if(buf.bpp != 32) return;
    fmbuf_fill(&buf, 0xFF212121);
    // fmbuf_draw_rect(&buf, 2, 2, buf.width/2, buf.height/2, 0xFF00FF00);
    // assert(limine_framebuffer_request.response && "No framebuffer :(");
    // assert(limine_framebuffer_request.response->framebuffer_count == 1 && limine_framebuffer_request.response->framebuffers[0] -> bpp == 32 && "Unsupported framebuffer format");
    // struct limine_framebuffer* fb = limine_framebuffer_request.response->framebuffers[0];
    // for(size_t y = 0; y < fb->height; ++y) {
    //     for(size_t x = 0; x < fb->width; ++x) {
    //         *((uint32_t*)((uint8_t*)fb->address + y*fb->pitch)+x) = 0xFF00FF00;//0xFF212121;
    //     }
    // }
}

#include "../embed.h"
static void init_rootfs() {
    intptr_t e = 0;
    const char* path = NULL;
    path = "/devices";
    if((e = vfs_mkdir(path)) < 0) {
        printf("ERROR: init_rootfs: Could not create %s : %s\n",path,status_str(e));
        kabort();
    }
    for(size_t i = 0; i < embed_entries_count; ++i) {
        EmbedEntry* entry = &embed_entries[i];
        switch(entry->kind) {
        case EMBED_FILE: {
            if((e = vfs_create(entry->name)) < 0) {
               printf("ERROR: init_rootfs: Could not create %s : %s\n",entry->name,status_str(e));
               kabort();
            }

            VfsFile file = {0};
            if((e = vfs_open(entry->name, &file, MODE_WRITE)) < 0) {
                printf("ERROR: init_rootfs: Could not open %s : %s\n",entry->name,status_str(e));
                vfs_close(&file);
                kabort();
            }
            if((e = write_exact(&file, embed_data+entry->offset, entry->size)) < 0) {
                printf("ERROR: init_rootfs: Could not write data: %s : %s\n",entry->name, status_str(e));
                vfs_close(&file);
                kabort();
            }
            vfs_close(&file);
        } break;
        case EMBED_DIR: {
            if((e = vfs_mkdir(entry->name)) < 0) {
                printf("ERROR: init_rootfs: Could not create %s : %s\n",entry->name,status_str(e));
                kabort();
            }
        } break;
        }
    }
#if 1
    path = "/Welcome.txt";
    if((e = vfs_create(path)) < 0) {
        printf("ERROR: init_rootfs: Could not create %s : %s\n",path,status_str(e));
        kabort();
    }
    {
        const char* msg = "Hello and welcome to MinOS!\nThis is a mini operating system made in C.\nDate of compilation " __DATE__ ".";
        VfsFile file = {0};
        if((e = vfs_open(path, &file, MODE_WRITE)) < 0) {
            printf("WARN: init_rootfs: Could not open %s : %s\n",path,status_str(e));
            log_cache(kernel.inode_cache);
            kabort();
        } else {
            if((e = write_exact(&file, msg, strlen(msg))) < 0) {
                vfs_close(&file);
                printf("WARN: init_rootfs: Could not write welcome message : %s\n",status_str(e));
            }
        } 
        vfs_close(&file);
    }
#endif
}


void dev_test() {
    const char* path = "/devices/vga0";
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
    init_memmap();
    init_paging();
    KERNEL_SWITCH_VTABLE();
    update_post_paging();
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
    serialDevice.init();
    vfs_register_device("serial0", &serialDevice);

    init_tasks();
    init_kernel_task();
    init_task_switch();
    init_syscalls();
    init_resources();
    pit_set_count(1000);
    assert(kernel.device_cache = create_new_cache(sizeof(Device)));
    fbt();
    init_vga();
    // dev_test();


    intptr_t e = 0;
    const char* epath = NULL;
    epath = "/user/nothing";
    if((e = exec(epath)) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }

    epath = "/user/syscall_test";
    if((e = exec(epath)) < 0) {
        printf("Failed to exec %s : %s\n",epath,status_str(e));
        kabort();
    }
    pic_clear_mask(0);
    for(;;) {
        asm volatile("hlt");
    }
}
