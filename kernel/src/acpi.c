#include "acpi.h"
#include "bootutils.h"
#include "log.h"
#include "filelog.h"
#include "kernel.h"
#include "iomem.h"

typedef struct {
    RSDP* rsdp;
    void* sdt;
} ACPI;
static ACPI acpi;
ACPISDTHeader* sdt_map_and_find(paddr_t phys, const char* signature) {
    if(!phys) return NULL;
    ACPISDTHeader* h;
    if(!(h=iomap_bytes(phys, sizeof(ACPISDTHeader), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
        kerror("(sdt_map_and_find) Failed to map acpi header");
        return NULL;
    }
    if(memcmp(h->signature, signature, 4) != 0) {
        iounmap_bytes(h, sizeof(ACPISDTHeader));
        return NULL;
    }
    size_t length = h->length;
    iounmap_bytes(h, sizeof(ACPISDTHeader));
    if(!(h=iomap_bytes(phys, length, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
        kerror("(sdt_map_and_find) Failed to remap acpi header");
        return NULL;
    }
    return h;
}
static ACPISDTHeader* rsdt_find(RSDT* rsdt, const char* signature) {
    ACPISDTHeader* h;
    size_t entries = (rsdt->header.length-sizeof(ACPISDTHeader)) / sizeof(uint32_t);
    for(size_t i = 0; i < entries; ++i) {
        if((h=sdt_map_and_find((paddr_t)((uint32_t*)(rsdt+1))[i], signature))) 
            return h;
    }
    return h;
}
static ACPISDTHeader* xsdt_find(XSDT* xsdt, const char* signature) {
    ACPISDTHeader* h;
    size_t entries = (xsdt->header.length-sizeof(ACPISDTHeader)) / sizeof(uint64_t);
    for(size_t i = 0; i < entries; ++i) {
        if((h=sdt_map_and_find((paddr_t)((uint32_t*)(xsdt+1))[i], signature))) 
            return h;
    }
    return h;
}
ACPISDTHeader* acpi_find(const char* signature) {
    if(acpi.rsdp->revision < 1)
        return rsdt_find(acpi.sdt, signature);
    return xsdt_find(acpi.sdt, signature);
}
static uint8_t count_checksum(const uint8_t* bytes, size_t count) {
    uint8_t checksum = 0;
    for(size_t i = 0; i < count; ++i) {
        checksum += bytes[i];
    }
    return checksum;
}
void init_acpi_isdt(paddr_t phys) {
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
        goto checksum_err;
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
    iounmap_bytes(h, length);
    return;
checksum_err:
    iounmap_bytes(h, length);
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
        init_acpi_isdt((paddr_t)((uint64_t*)(xsdt+1))[i]);
    }
}
void init_rsdt(RSDT* rsdt) {
    size_t entries = (rsdt->header.length-sizeof(ACPISDTHeader)) / sizeof(uint32_t);
    Logger* oldlogger = kernel.logger;
    const char* oldpath = file_logger.private;
    file_logger.private = "/RSDT.log";
    kernel.logger = &file_logger;
    kinfo("Signature of header: %.4s", rsdt->header.signature);
    kinfo("Total amount of entries: %zu", entries);
    file_logger.private = (void*)oldpath;
    kernel.logger = oldlogger;
    for(size_t i = 0; i < entries; ++i) {
        init_acpi_isdt((paddr_t)((uint32_t*)(rsdt+1))[i]);
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
    acpi.sdt = xsdt;
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
void init_rsdp(RSDP* rsdp) {
    Logger* oldlogger = kernel.logger;
    const char* oldpath = file_logger.private;
    file_logger.private = "/RSDP.log";
    kernel.logger = &file_logger;
    kinfo("Signature: %.8s", rsdp->signature);
    kinfo("oemid: %.6s", rsdp->OEMID);
    kinfo("revision: %d", rsdp->revision);
    kinfo("rsdt: %p", (void*)(uintptr_t)rsdp->rsdt_address); 
    RSDT* rsdt;
    if(!(rsdt=iomap_bytes(rsdp->rsdt_address, sizeof(RSDT), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
        kerror("Not enough memory for rsdt");
        return;
    }
    size_t length_rsdt = rsdt->header.length;
    kinfo("length=%zu", length_rsdt);
    iounmap_bytes(rsdt, sizeof(RSDT));
    if(!(rsdt=iomap_bytes(rsdp->rsdt_address, length_rsdt, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE))) {
        kerror("Not enough memory for rsdt (2)");
        return;
    }
    acpi.sdt = rsdt;
    file_logger.private = (void*)oldpath;
    kernel.logger = oldlogger;
    init_rsdt(rsdt);
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
    acpi.rsdp = rsdp;
    if(rsdp->revision > 1) {
        init_xsdp((XSDP*)rsdp);
    } else {
        init_rsdp(rsdp);
    }
}
