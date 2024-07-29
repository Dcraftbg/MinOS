#pragma once
#include "gdt.h"
#include "memory.h"
#include "utils.h"
#include <stdint.h>
typedef struct {
    uint16_t base_low;
    uint16_t segment;
    uint16_t tag; // P + DPL + 0 + Gate Type + Reserved + IST
    uint16_t base_middle;
    uint32_t base_high;
    uint32_t reserved;
} IDTEntry;
typedef struct {
    IDTEntry inner[256];
} IDT;
typedef struct {
    uint16_t size;
    uint64_t addr;
} __attribute__((packed)) IDTDescriptor;

#define IDT_EXCEPTION_TYPE 0x8E
#define IDT_GATE_TYPE      0x8E
#define IDT_HARDWARE_TYPE  0x8F
#define IDT_SOFTWARE_TYPE  0xEF

typedef void (*IDTHandler_t);
void idt_pack_entry(IDTEntry* entry, IDTHandler_t handler, uint8_t typ);
void init_idt();
#define idt_register(what, handler, typ) idt_pack_entry(&kernel.idt->inner[what], handler, typ);

#define disable_interrupts() __asm__ volatile("cli")
#define enable_interrupts() __asm__ volatile("sti")
