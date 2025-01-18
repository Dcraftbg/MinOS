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
#define PCI_CMD_IO_SPACE     0b1
#define PCI_CMD_MEM_SPACE    0b10
#define PCI_CMD_BUS_MASTER   0b100
#define PCI_CMD_CYCLES       0b1000
#define PCI_CMD_MEM_WRITE    0b10000
#define PCI_CMD_INVAL_ENABLE PCI_STATUS_MEM_WRITE
#define PCI_CMD_VGA_PALSNOOP 0b100000
#define PCI_CMD_PARITY_ERR   0b1000000

#define PCI_STATUS_INT 0b1000
#define PCI_STATUS_CAP 0b10000

#define PCI_CAP_START  0x34
#define PCI_CAPID_MSI  0x05
#define PCI_CAPID_MSIX 0x11
typedef struct {
    uint8_t bus, slot, func;
    uint16_t vendor_id;
    uint16_t status, command;
    uint16_t device;
    uint8_t base_class, subclass, prog_inferface, revision_id;
    uint8_t msi_offset, msi_x_offset;
    Bar bar0;
} PciDevice;
void pci_device_enum_caps(PciDevice* dev);
void pci_device_msi_disable(PciDevice* dev);
void pci_device_msi_enable(PciDevice* dev);
void pci_device_msi_set(PciDevice* dev, uintptr_t addr, uint8_t vec);
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
bool pci_add(Pci* pci, PciDevice* device);
// TODO: Maybe consider moving this to Kernel
extern Pci pci;
