#pragma once
#include "utils.h"
#include <stdint.h>
#include <stddef.h>
// NOTE: IRQs are offset by 0x20 to remove the exception range
#define PIC_BASE  (0x20-0x20)
#define PIC_COUNT 16
#define MSI_BASE (PIC_BASE+PIC_COUNT)
#define MSI_COUNT 48
#define MSI_BYTES_COUNT ((MSI_COUNT+7)/8)
static_assert(MSI_COUNT % 8 == 0, "MSI_COUNT needs to be byte aligned because of reasons");
#define MSI_ADDRESS 0xFEE00000
typedef struct {
    uint8_t irqs[MSI_BYTES_COUNT];
} MSIManager;
intptr_t msi_reserve_irq(MSIManager* manager);
// TODO: Consider moving into Kernel
extern MSIManager msi_manager;
