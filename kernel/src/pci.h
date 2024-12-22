#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset); 
void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t what); 
// ^--- Core PCI functions
typedef struct {
    bool mmio;
    union {
        struct {
            bool prefetch;
            void* addr;
        } mmio;
        uint32_t port;
    } as;
    uint32_t size;
} Bar;  
typedef struct {
    uint16_t vendor_id;
    uint16_t status, command;
    uint16_t device;
    uint8_t base_class, subclass, prog_inferface, revision_id;
    Bar bar0;
} PciDevice;
#define PCI_BUS_COUNT  256
#define PCI_SLOT_COUNT 32
#define PCI_DEV_COUNT  8
typedef PciDevice* PciSlot /*[PCI_DEV_COUNT]*/;
typedef PciSlot* PciBus /*[PCI_SLOT_COUNT]*/;
typedef struct {
    PciBus* busses[PCI_BUS_COUNT];
} Pci;

intptr_t pci_map_bar0(Bar* bar, size_t bus, size_t slot, size_t func);
intptr_t pci_scan(Pci* pci);
void init_pci();
bool pci_add(Pci* pci, size_t bus, size_t slot, size_t func, PciDevice* device);
// TODO: Maybe consider moving this to Kernel
extern Pci pci;
