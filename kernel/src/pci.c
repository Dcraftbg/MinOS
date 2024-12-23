#include "pci.h"
#include "port.h"
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
   uint32_t address = (uint32_t)(((uint32_t)bus) << 16) | (((uint32_t)slot)<<11) | 
           (((uint32_t)func) << 8) | (offset & 0xFC) | ((uint32_t)0x80000000);
   outl(0xCF8, address);
   return (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
   uint32_t address = (uint32_t)(((uint32_t)bus) << 16) | (((uint32_t)slot)<<11) | 
           (((uint32_t)func) << 8) | (offset & 0xFC) | ((uint32_t)0x80000000);
   outl(0xCF8, address);
   return inl(0xCFC);
}
void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t what) {
   uint32_t address = (uint32_t)(((uint32_t)bus) << 16) | (((uint32_t)slot)<<11) | 
           (((uint32_t)func) << 8) | (offset & 0xFC) | ((uint32_t)0x80000000);
   outl(0xCF8, address);
   outl(0xCFC, what);
}
#include "kernel.h"
#include "log.h"
#include "filelog.h"
#include "iomem.h"
static inline Cache* pci_bus_cache() {
    return kernel.cache256;
}
static PciBus* pci_bus_new() {
    PciBus* bus = cache_alloc(pci_bus_cache());
    if(!bus) return NULL;
    memset(bus, 0, sizeof(PciSlot*)*PCI_SLOT_COUNT);
    return bus;
}
static inline Cache* pci_slot_cache() {
    return kernel.cache64;
}
static PciSlot* pci_slot_new() {
    PciSlot* slot = cache_alloc(pci_slot_cache());
    if(!slot) return NULL;
    memset(slot, 0, sizeof(PciDevice*)*PCI_DEV_COUNT);
    return slot;
}
static inline PciDevice* pci_device_new() {
    return cache_alloc(kernel.pci_device_cache);
}
static inline void pci_device_destroy(PciDevice* device) {
    return cache_dealloc(kernel.pci_device_cache, device);
}
bool pci_add(Pci* pci, size_t bus, size_t slot, size_t func, PciDevice* device) {
    if(!pci->busses[bus]) 
        if(!(pci->busses[bus] = pci_bus_new())) return false;
    if(!pci->busses[bus][slot])
        if(!(pci->busses[bus][slot] = pci_slot_new())) return false;
    pci->busses[bus][slot][func] = device;
    return true;
}
intptr_t pci_map_bar0(Bar* bar, size_t bus, size_t slot, size_t func) {
    uint32_t bar0_base_addr = pci_config_read_dword(bus, slot, func, 0x10);
    if(!bar0_base_addr) {
        bar->mmio = true;
        bar->as.mmio.prefetch = false;
        bar->as.mmio.addr = NULL;
        return 0;
    }
    pci_config_write_dword(bus, slot, func, 0x10, 0xFFFFFFFF);
    bar->size = (~(pci_config_read_dword(bus, slot, func, 0x10) & 0xfffff000)) + 1;
    pci_config_write_dword(bus, slot, func, 0x10, bar0_base_addr);
    if(bar0_base_addr & 0b1) {
        bar->mmio = false;
        bar->as.port = bar0_base_addr >> 2;
        return 0;
    }
    // NOTE: Kind of wasteful cuz its mapping stuff that is already kind of accessible I guess.
    // For now its fine tho
    uint8_t type = (bar0_base_addr>>1) & 0b11;
    bar->mmio = true;
    bar->as.mmio.prefetch = (bar0_base_addr >> 3) & 0b1;
    paddr_t bar0_phys=0;
    switch(type) {
    case 2:
        bar0_phys |= ((paddr_t)pci_config_read_dword(bus, slot, 0, 0x14)) << 32;
    case 0: 
        bar0_phys |= bar0_base_addr & 0xFFFFFFF0;
        break;
    default:
        return -INVALID_TYPE;
    }
    if(!(bar->as.mmio.addr = iomap_bytes(bar0_phys, bar->size, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT)))
        return -NOT_ENOUGH_MEM;
    return 0;
}
intptr_t pci_scan(Pci* pci) {
    for(size_t bus = 0; bus < PCI_BUS_COUNT; ++bus) {
        for(size_t slot = 0; slot < PCI_SLOT_COUNT; ++slot) {
            uint16_t vendor_id = pci_config_read_word(bus, slot, 0, 0);
            if(vendor_id == 0xFFFF) continue;
            PciDevice* device = pci_device_new();
            if(!device) {
                kerror("(pci) Not enough memory to create PCI device");
                return -NOT_ENOUGH_MEM;
            }
            if(!pci_add(pci, bus, slot, 0, device)) {
                kerror("(pci) Failed to add device. Not enough memory");
                pci->busses[bus][slot][0] = NULL;
                pci_device_destroy(device);
                return -NOT_ENOUGH_MEM;
            }
            device->vendor_id = vendor_id;
            uint32_t status_cmd = pci_config_read_dword(bus, slot, 0, 0x4);
            device->status  = (uint16_t)(status_cmd >> 16);
            device->command = (uint16_t)(status_cmd & 0xFFFF);
            device->device = pci_config_read_word(bus, slot, 0, 2);

            uint32_t general = pci_config_read_dword(bus, slot, 0, 8);
            device->base_class     = general >> 24;
            device->subclass       = general >> 16;
            device->prog_inferface = general >> 8;
            device->revision_id    = general;
            
            Logger* oldlogger = kernel.logger;
            const char* oldpath = file_logger.private;
            file_logger.private = "/pci/scan.log";
            kernel.logger = &file_logger;
            kinfo("(bus=%03d slot=%02d) vid=%04X device=%04X subclass=%02X", bus, slot, device->vendor_id, device->device, device->subclass);
            file_logger.private = (void*)oldpath;
            kernel.logger = oldlogger;

            intptr_t e;
            if((e=pci_map_bar0(&device->bar0, bus, slot, 0)) < 0) {
                kerror("(pci) Failed to map bar: %s", status_str(e));
                pci->busses[bus][slot][0] = NULL;
                pci_device_destroy(device);
                return e;
            }
        }
    }

    return 0;
}


#include <usb/usb.h>
void init_pci() {
    assert(kernel.pci_device_cache = create_new_cache(sizeof(PciDevice), "PciDevice"));
    intptr_t e;
    if((e=vfs_creat_abs("/pci", O_DIRECTORY)) < 0)
        kwarn("(pci) Failed to create pci folder: %s", status_str(e));
    
    if((e=pci_scan(&pci)) < 0)
        kpanic("(pci) Failure on pci scan: %s", status_str(e));
    for(size_t busi = 0; busi < PCI_BUS_COUNT; ++busi) {
        PciBus* bus = pci.busses[busi];
        if(!bus) continue;
        for(size_t sloti = 0; sloti < PCI_SLOT_COUNT; ++sloti) {
            PciSlot* slot = bus[sloti];
            if(!slot) continue;
            for(size_t devi = 0; devi < PCI_DEV_COUNT; ++devi) {
                PciDevice* dev = slot[devi];
                if(!dev) continue;
                if(dev->subclass == 0x03) {
                    Logger* oldlogger = kernel.logger;
                    const char* oldpath = file_logger.private;
                    file_logger.private = "/pci/usb.log";
                    kernel.logger = &file_logger;
                    intptr_t e;
                    kinfo("usb (bus=%03d slot=%02d device=%d). (pi=0x%02X)", busi, sloti, devi, dev->prog_inferface);
                    if((e=init_usb(dev)) < 0) {
                        kwarn("usb Failed to initialise USB device: %s", status_str(e));
                        continue;
                    }
                    file_logger.private = (void*)oldpath;
                    kernel.logger = oldlogger;
                }
            }
        }
    }
}
Pci pci;
