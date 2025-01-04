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
    MADT_ENTRY_LOCAL_APIC = 0,
    MADT_ENTRY_IOAPIC = 1,
};
typedef struct {
    APICEntry entry;
    uint8_t  ioapic_id;
    uint8_t  _reserved;
    uint32_t ioapic_addr;
    uint32_t gsi;
} IOApicEntry;
static_assert(sizeof(IOApicEntry) == 12, "Invalid padding in IOApicEntry");
static uint32_t ioapic_read(void *ioapic_addr, uint32_t reg) {
    uint32_t volatile *ioapic = (uint32_t volatile*)ioapic_addr;
    ioapic[0] = (reg & 0xFF);
    return ioapic[4];
}
static void ioapic_write(void *ioapic_addr, uint32_t reg, uint32_t value) {
    uint32_t volatile *ioapic = (uint32_t volatile*)ioapic_addr;
    ioapic[0] = (reg & 0xFF);
    ioapic[4] = value;
}
typedef struct {
    void   *addr;
    uint32_t gsi;
} IOApic;
#define IOAPIC_ADDR_SPACE_SIZE 64
IOApic ioapic = {0};
typedef uint8_t ioapic_flags_t;
#define IOAPIC_FLAG_LOW   0b1
#define IOAPIC_FLAG_HIGH  0b0
#define IOAPIC_FLAG_LEVEL 0b10
#define IOAPIC_FLAG_EDGE  0b00
#define IOAPIC_REDIRECTION_BASE 0x10
void ioapic_register(uint8_t vec, size_t irq, uint8_t lapic_id, ioapic_flags_t flags) {
    size_t number = ioapic.gsi + (irq * 2);
    uint32_t reg_low  = ioapic_read(ioapic.addr, IOAPIC_REDIRECTION_BASE + number),
             reg_high = ioapic_read(ioapic.addr, IOAPIC_REDIRECTION_BASE + number + 1);
    reg_low  = (reg_low  & ~0xFF) | vec;
    // Set delivery mode to normal + destination mode = physical
    reg_low  = (reg_low  & ~(0x700 | 0x800));
    reg_low  = (reg_low  & ~0x2000) | (flags & IOAPIC_FLAG_LOW);
    reg_low  = (reg_low  & ~0x8000) | (flags & IOAPIC_FLAG_LEVEL);
    reg_low  = (reg_low  & ~0x10000);
    reg_high = (reg_high & ~0xf0000000) | (((uint32_t)lapic_id) << 28);
    ioapic_write(ioapic.addr, IOAPIC_REDIRECTION_BASE + number    , reg_low);
    ioapic_write(ioapic.addr, IOAPIC_REDIRECTION_BASE + number + 1, reg_high);
}
void ioapic_set_mask(size_t irq, uint32_t on) {
    size_t number = ioapic.gsi + (irq * 2);
    uint32_t reg_low  = ioapic_read(ioapic.addr, IOAPIC_REDIRECTION_BASE + number);
    reg_low  = (reg_low & ~0x10000) | (on << 16);
    ioapic_write(ioapic.addr, IOAPIC_REDIRECTION_BASE + number    , reg_low);
}
void init_apic() {
    ACPISDTHeader* apic_header = acpi_find("APIC"); 
    if(!apic_header) return;
    if(apic_header->length < sizeof(APIC)) {
        kerror("(APIC) Length was odd");
        goto length_check_err;
    }
    APIC* apic = (APIC*)apic_header;
    for(
        APICEntry* entry = (APICEntry*)(apic+1);
        ((char*)entry < (((char*)apic) + apic->header.length));
        entry = (APICEntry*)(((char*)entry) + entry->length)
    ) {
        if(entry->type == MADT_ENTRY_IOAPIC) {
            if(entry->length < sizeof(IOApicEntry)) {
                kerror("IOAPIC entry with size < %zu (size=%zu)", sizeof(IOApicEntry), entry->length);
                continue;
            }
            IOApicEntry* ioapic_entry = (IOApicEntry*)entry;
            void* new_ioapic_addr = iomap_bytes(ioapic_entry->ioapic_addr, IOAPIC_ADDR_SPACE_SIZE, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT);
            if(!new_ioapic_addr) {
                kerror("IOAPIC not enough memory to map it in");
                continue;
            }
            if(ioapic.addr)
                iounmap_bytes(ioapic.addr, IOAPIC_ADDR_SPACE_SIZE);
            ioapic.addr = new_ioapic_addr;
            ioapic.gsi  = ioapic_entry->gsi;
            kinfo("Mapped in ioapic!");
        }
        kinfo("APIC entry:");
        kinfo(" - type: %d", entry->type);
        kinfo(" - length: %zu", entry->length);
    }
    return;
length_check_err:
    iounmap_bytes(apic_header, apic_header->length);
}
