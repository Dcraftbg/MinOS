#include "acpi.h"
#include "bootutils.h"
#include "log.h"
#include "filelog.h"
#include "kernel.h"
#include "iomem.h"

static uint8_t count_checksum(const uint8_t* bytes, size_t count) {
    uint8_t checksum = 0;
    for(size_t i = 0; i < count; ++i) {
        checksum += bytes[i];
    }
    return checksum;
}
void init_acpi_sdt(paddr_t phys) {
    if(!phys) {
        kwarn("Ignoring phys=NULL");
        return;
    }
    ACPISDTHeader* h;
    if(!(h=iomap_bytes(phys, sizeof(ACPISDTHeader), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
        kerror("Failed to mmap acpi header");
        return;
    }
    size_t length = h->length;
    iounmap_bytes(h, sizeof(ACPISDTHeader));
    if(!(h=iomap_bytes(phys, length, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
        kerror("Failed to mmap acpi header (2)");
        return;
    }
    uint8_t checksum = count_checksum((uint8_t*)h, length);
    if(checksum != 0) {
        kwarn("checksum mismatch: Expected 0 found `%d`", checksum);
        return;
    }
    Logger* oldlogger = kernel.logger;
    const char* oldpath = file_logger.private;
    char name[20]={0};
    memcpy(name+0 , "/acpi/"    , 6);
    memcpy(name+6 , h->signature, 4);
    memcpy(name+10, ".log"      , 5);
    file_logger.private = name;
    kernel.logger = &file_logger;
    kinfo("signature: %.4s", h->signature);
    kinfo("length: %zu", (size_t)h->length);
    kinfo("revision: %d", h->revision);
    kinfo("oemid: %.6s", h->OEMID);
    kinfo("oemtableid: %.8s", h->OEMTABLEID);
    kinfo("OEMRevision: %d", h->OEMRevision);
    kinfo("CreatorID: %d", h->CreatorID);
    kinfo("CreatorRevision: %d", h->CreatorRevision);
    file_logger.private = (void*)oldpath;
    kernel.logger = oldlogger;
}
void init_xsdt(XSDT* xsdt) {
    size_t entries = (xsdt->header.length-sizeof(ACPISDTHeader)) / sizeof(uint64_t);
    Logger* oldlogger = kernel.logger;
    const char* oldpath = file_logger.private;
    file_logger.private = "/XSDT.log";
    kernel.logger = &file_logger;
    kinfo("Signature of header: %.4s", xsdt->header.signature);
    kinfo("Total amount of entries: %zu", entries);
    file_logger.private = (void*)oldpath;
    kernel.logger = oldlogger;
    for(size_t i = 0; i < entries; ++i) {
        init_acpi_sdt(((paddr_t*)(xsdt+1))[i]);
    }
}
void init_xsdp(XSDP* xsdp) {
    uint8_t checksum = count_checksum((uint8_t*)xsdp->__xsdp_2, sizeof(XSDP)-sizeof(RSDP));
    if(checksum != 0) {
        kwarn("checksum mismatch: Expected 0 found `%d` (xsdp)", checksum);
        return;
    }
    Logger* oldlogger = kernel.logger;
    const char* oldpath = file_logger.private;
    file_logger.private = "/XSDP.log";
    kernel.logger = &file_logger;
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
    file_logger.private = (void*)oldpath;
    kernel.logger = oldlogger;
    init_xsdt(xsdt);
}
void init_acpi() {
    intptr_t e;
    if((e=vfs_creat_abs("/acpi", O_DIRECTORY)) < 0) {
        kwarn("Failed to create /acpi: %s", status_str(e));
        return;
    }
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
        init_xsdp((XSDP*)rsdp);
    } else {
        kinfo("Signature: %.8s", rsdp->signature);
        kinfo("oemid: %.6s", rsdp->OEMID);
        kinfo("revision: %d", rsdp->revision);
        kinfo("rsdt: %p", (void*)(uintptr_t)rsdp->rsdt_address);
    }
}
