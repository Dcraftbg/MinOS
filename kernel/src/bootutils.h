#pragma once
#include "assert.h"
#include "framebuffer.h"
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uintptr_t virt;
    uintptr_t phys;
} Mempair;
size_t get_framebuffer_count();
Framebuffer get_framebuffer_by_id(size_t id);
void kernel_file_data(void** data, size_t* size);
void kernel_mempair(Mempair* mempair);    // Gives the phys and virt address of the kernel
void boot_map_phys_memory(); // Map physical memory into virtual memory. Bootloader specific as it uses the responses from the bootloader itself
typedef struct {
    const char* path;
    const char* cmdline;
    size_t size;
    char* data;
} BootModule;
size_t get_bootmodules_count();
bool get_bootmodule(size_t i, BootModule* module);
bool find_bootmodule(const char* path, BootModule* module);
char* get_kernel_cmdline();
#include "page.h"
paddr_t get_rsdp_addr();
#define PHYS_RAM_MIRROR_SIZE (4LLU * 1024LLU * 1024LLU * 1024LLU) 
