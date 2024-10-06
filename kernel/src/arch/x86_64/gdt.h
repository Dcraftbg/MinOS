#pragma once
#include <stdint.h>
#include <stddef.h> // offsetof
typedef struct {
    uint64_t null;
    uint64_t kernelCode;
    uint64_t kernelData;
    uint64_t userCode;
    uint64_t userData;
    uint64_t tss[2];
} GDT;

#define GDT_KERNELDATA offsetof(GDT, kernelData)
#define GDT_KERNELCODE offsetof(GDT, kernelCode)
#define GDT_USERDATA   offsetof(GDT, userData)
#define GDT_USERCODE   offsetof(GDT, userCode)
#define GDT_TSS        offsetof(GDT, tss)
typedef struct {
    uint16_t size;
    GDT* addr;
} __attribute__((packed)) GDTDescriptor;

#define GDT_PACK_ENTRY(base, limit, flags, access) \
(\
      (((base >> 24  ) & 0xFF  ) << 56)\
    | ((((flags)     ) & 0xF   ) << 52)\
    | ((((limit) >>16) & 0xF   ) << 48)\
    | (((access)     )           << 40)\
    | ((((base ) >> 8) & 0xFF  ) << 32)\
    | ((((base )     ) & 0xFFFF) << 16)\
    | ((((limit)     ) & 0xFFFF)      )\
)
extern void kernel_reload_gdt_registers();
void init_gdt();
