#pragma once
#include "../../../../config.h"
#include "gdt.h"
#include "memory.h"
#include "utils.h"
#include "interrupt.h"
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

#define IDT_INTERRUPT_TYPE 0x8E
#define IDT_TRAP_TYPE  0x8F
#define IDT_SOFTWARE_TYPE  0xEF

typedef void (*IDTHandler_t);
void idt_pack_entry(IDTEntry* entry, IDTHandler_t handler, uint8_t typ);
void reload_idt(void);
void init_idt();
void irq_set_handler(size_t id, IrqHandler handler);

#define disable_interrupts() __asm__ volatile("cli")
#define enable_interrupts() __asm__ volatile("sti")
