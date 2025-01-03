#include "iomem.h"
#include "log.h"
#include "acpi.h"

typedef struct {
    ACPISDTHeader header;
    uint32_t lapic_addr;
    uint32_t flags;
} APIC;
typedef struct {
    uint8_t type;
    uint8_t length;
} APICEntry;
enum {
    IOAPIC = 0,
    LOCAL_APIC = 1
};
void init_apic() {
    ACPISDTHeader* apic_header = acpi_find("APIC"); 
    if(!apic_header) return;
    if(apic_header->length < sizeof(APIC)) {
        kerror("(APIC) Length was odd");
        goto length_check_err;
    }
    APIC* apic = (APIC*)apic_header;
    APICEntry* entry = (APICEntry*)(apic+1);
    while((char*)entry < (((char*)apic) + apic->header.length)) {
        kinfo("APIC entry:");
        kinfo(" - type: %d", entry->type);
        kinfo(" - length: %zu", entry->length);
        entry = (APICEntry*)(((char*)entry) + entry->length);
    }
    return;
length_check_err:
    iounmap_bytes(apic_header, apic_header->length);
}
