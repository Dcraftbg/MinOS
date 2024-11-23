#include "acpi.h"
#include "bootutils.h"
#include "log.h"
#include "kernel.h"
#include "iomem.h"

void init_acpi() {
    paddr_t rsdp_phys = get_rsdp_addr();
    if(!rsdp_phys) {
        kwarn("RSDP pointer not supplied");
        return;
    }
                         // NOTE: We map it as XSDP because I'm too lazy to map and unmap for XSDP
    RSDP* rsdp = iomap_bytes(rsdp_phys, sizeof(XSDP), KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT);
    if(!rsdp) {
        kerror("Not enough memory for rsdp");
        return;
    }
    {
        uint8_t* rsdp_bytes = (uint8_t*)rsdp;
        uint8_t checksum = 0;
        for(size_t i = 0; i < sizeof(RSDP); ++i) {
            checksum += rsdp_bytes[i];
        }
        if(checksum != 0) {
            kwarn("checksum mismatch: Expected 0 found `%d`", checksum);
            return;
        }
    }
    if(rsdp->revision > 1) {
        XSDP* xsdp = (XSDP*)rsdp;
        {
            uint8_t* xsdp_bytes = (uint8_t*)xsdp->__xsdp_2;
            uint8_t checksum = 0;
            for(size_t i = 0; i < sizeof(XSDP)-sizeof(RSDP); ++i) {
                checksum += xsdp_bytes[i];
            }
            if(checksum != 0) {
                kwarn("checksum mismatch: Expected 0 found `%d`", checksum);
                return;
            }
        }
        kinfo("Signature: %.8s", xsdp->signature);
        kinfo("oemid: %.6s", xsdp->OEMID);
        kinfo("revision: %d", xsdp->revision);
        kinfo("rsdt: %p", (void*)(uintptr_t)xsdp->rsdt_address); 
        kinfo("length: %zu", (size_t)xsdp->length);
        kinfo("xsdt_addr: %p", (void*)xsdp->xsdt_address);
        XSDT* xsdt;
        if(!(xsdt=iomap_bytes(xsdp->xsdt_address, sizeof(XSDT), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
            kerror("Not enough memory for xsdt");
            return;
        }
        size_t length_xsdt = xsdt->header.length;
        kinfo("length=%zu", length_xsdt);
        iounmap_bytes(xsdt, sizeof(XSDT));
        if(!(xsdt=iomap_bytes(xsdp->xsdt_address, length_xsdt, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
            kerror("Not enough memory for xsdt (2)");
            return;
        }
        size_t entries = (xsdt->header.length-sizeof(ACPISDTHeader)) / sizeof(uint64_t);
        kinfo("Signature of header: %.4s", xsdt->header.signature);
        kinfo("Total amount of entries: %zu", entries);
        for(size_t i = 0; i < entries; ++i) {
            paddr_t h_phys = ((paddr_t*)(xsdt+1))[i];
            if(!h_phys) {
                kwarn("Ignoring phys=NULL");
                continue;
            }
            ACPISDTHeader* h;
            if(!(h=iomap_bytes(h_phys, sizeof(ACPISDTHeader), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
                kerror("Failed to mmap acpi header");
                return;
            }
            size_t length_h = h->length;
            iounmap_bytes(h, sizeof(ACPISDTHeader));
            if(!(h=iomap_bytes(h_phys, length_h, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
                kerror("Failed to mmap acpi header (2)");
                return;
            }
            {
                uint8_t* h_bytes = (uint8_t*)h;
                uint8_t checksum = 0;
                for(size_t i = 0; i < h->length; ++i) {
                    checksum += h_bytes[i];
                }
                if(checksum != 0) {
                    kwarn("checksum mismatch: Expected 0 found `%d`", checksum);
                    continue;
                }
            }
            kinfo("signature: %.4s", h->signature);
            kinfo("length: %zu", (size_t)h->length);
            kinfo("revision: %d", h->revision);
            kinfo("oemid: %.6s", h->OEMID);
            kinfo("oemtableid: %.8s", h->OEMTABLEID);
            kinfo("OEMRevision: %d", h->OEMRevision);
            kinfo("CreatorID: %d", h->CreatorID);
            kinfo("CreatorRevision: %d", h->CreatorRevision);
        }
    } else {
        kinfo("Signature: %.8s", rsdp->signature);
        kinfo("oemid: %.6s", rsdp->OEMID);
        kinfo("revision: %d", rsdp->revision);
        kinfo("rsdt: %p", (void*)(uintptr_t)rsdp->rsdt_address);
    }
}
