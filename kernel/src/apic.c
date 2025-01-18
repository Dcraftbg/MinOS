#include "iomem.h"
#include "log.h"
#include "acpi.h"
#include "apic.h"

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
void* lapic_addr = NULL;

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
static uint32_t lapic_read(void *lapic_addr, size_t off) {
    uint32_t volatile *lapic = (uint32_t volatile*)(lapic_addr + off);
    return lapic[0];
}
static void lapic_write(void *lapic_addr, size_t off, uint32_t value) {
    uint32_t volatile *lapic = (uint32_t volatile*)(lapic_addr + off);
    lapic[0] = value;
}
static void lapic_eoi(void *lapic_addr2) {
    lapic_write(lapic_addr2, 0xB0, 0);
}

static uint8_t vec_bitmap[256/8] = {0};
static int vec_alloc(void) {
    for(size_t i = 0; i < 32; ++i) {
        if(vec_bitmap[i] == 0xFF) continue;
        size_t j = 0;
        while(vec_bitmap[i] & (1 << j)) j++;
        vec_bitmap[i] |= 1 << j;
        return i * 8 + j;
    }
    return -1;
}
static void vec_reserve(size_t vec) {
    size_t byte = vec / 8, bit = vec % 8;
    vec_bitmap[byte] |= bit;
}
#define LAPIC_LVT_TIMER_OFFSET 0x320
#define LAPIC_INITCNT_OFFSET   0x380
#define LAPIC_CURRCNT_OFFSET   0x390
// NOTE: Divide configuration register
#define LAPIC_DIV_OFFSET       0x3E0
#define LAPIC_TIMER_IRQ        0x28

#define LVT_MASK_SHIFT    (16)
#define LVT_MASK          (1 << LVT_MASK_SHIFT) 
#define LVT_TIMER_ONESHOT (0x20000)
static size_t tmp_pit_ticks = 0;
__attribute__((interrupt)) 
void _tmp_pit_handler(void *) {
    tmp_pit_ticks++;
    irq_eoi(0);
}
void init_apic() {
    memset(vec_bitmap, 0xFF, 4);
    // Reserve IRQ0 for lapic timer
    vec_reserve(LAPIC_TIMER_IRQ);
    // Reserve IRQ 0x80 for syscall
    vec_reserve(0x80);
    // Reserve IRQ 0xFF Spurious interrupt 
    vec_reserve(0xFF);
    ACPISDTHeader* apic_header = acpi_find("APIC"); 
    if(!apic_header) return;
    if(apic_header->length < sizeof(APIC)) {
        kerror("(APIC) Length was odd");
        goto length_check_err;
    }
    APIC* apic = (APIC*)apic_header;
    lapic_addr = iomap_bytes(apic->lapic_addr, 4096, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_WRITE_THROUGH);
    if(!lapic_addr) {
        kerror("LAPIC not enough memory to map in lapic");
        goto lapic_addr_err;
    }
    for(
        APICEntry* entry = (APICEntry*)(apic+1);
        ((char*)entry < (((char*)apic) + apic->header.length));
        entry = (APICEntry*)(((char*)entry) + entry->length)
    ) {
        switch(entry->type) {
        case MADT_ENTRY_IOAPIC: {
            if(entry->length < sizeof(IOApicEntry)) {
                kerror("IOAPIC entry with size < %zu (size=%zu)", sizeof(IOApicEntry), entry->length);
                continue;
            }
            IOApicEntry* ioapic_entry = (IOApicEntry*)entry;
            void* new_ioapic_addr = iomap_bytes(ioapic_entry->ioapic_addr, IOAPIC_ADDR_SPACE_SIZE, KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE_THROUGH);
            if(!new_ioapic_addr) {
                kerror("IOAPIC not enough memory to map it in");
                continue;
            }
            if(ioapic.addr)
                iounmap_bytes(ioapic.addr, IOAPIC_ADDR_SPACE_SIZE);
            ioapic.addr = new_ioapic_addr;
            ioapic.gsi  = ioapic_entry->gsi;
            kinfo("Mapped in ioapic!");
        } break;
        } 
        kinfo("APIC entry:");
        kinfo(" - type: %d", entry->type);
        kinfo(" - length: %zu", entry->length);
    }
    for(size_t i = 0x20; i < 0xFF; ++i) {
        ioapic_set_mask(i, 1);
    }
    pit_set_hz(1000);
    irq_register(0, _tmp_pit_handler, IRQ_FLAG_FAST);
    // APIC divider of 16
    lapic_write(lapic_addr, LAPIC_DIV_OFFSET    , 3);
    lapic_write(lapic_addr, LAPIC_INITCNT_OFFSET, 0xFFFFFFFF);
    irq_clear(0);
    // NOTE: For a little more accuracy we sleep for 10ms and then divide by 10
    while(tmp_pit_ticks < 10) asm volatile("hlt");
    size_t ticks = (0xFFFFFFFF - lapic_read (lapic_addr, LAPIC_CURRCNT_OFFSET)) / 10;
    lapic_write(lapic_addr, LAPIC_LVT_TIMER_OFFSET, LVT_TIMER_ONESHOT | LVT_MASK | LAPIC_TIMER_IRQ);
    lapic_write(lapic_addr, LAPIC_INITCNT_OFFSET  , ticks);
    // Disable PIC
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    // Enable APIC
    lapic_write(lapic_addr, 0x80, 0);
    lapic_write(lapic_addr, 0xE0, 0xF << 28);
    lapic_write(lapic_addr, 0xF0, 0xFF | 0x100);
    kernel.interrupt_controller = &apic_controller;
    kernel.task_switch_irq = LAPIC_TIMER_IRQ;
    return;
    iounmap_bytes(lapic_addr, 4096);
    lapic_addr = NULL;
lapic_addr_err:
length_check_err:
    iounmap_bytes(apic_header, apic_header->length);
}
intptr_t apic_reserve(IntController* _, size_t irq) {
    if(irq == LAPIC_TIMER_IRQ) return LAPIC_TIMER_IRQ;
    int vec = vec_alloc();
    if(vec < 0) return -LIMITS;
    ioapic_register(vec, irq, 0, IOAPIC_FLAG_HIGH | IOAPIC_FLAG_EDGE);
    return vec;
}
void apic_eoi(IntController* _, size_t _irq) {
    lapic_eoi(lapic_addr);
}
void apic_set_mask(IntController* _, size_t irq, uint32_t on) {
    if(irq == LAPIC_TIMER_IRQ) {
        uint32_t reg_low  = lapic_read(lapic_addr, LAPIC_LVT_TIMER_OFFSET);
        reg_low  = (reg_low & ~LVT_MASK) | (on << LVT_MASK_SHIFT);
        lapic_write(lapic_addr, LAPIC_LVT_TIMER_OFFSET, reg_low);
    } else {
        ioapic_set_mask(irq, on);
    }
}
IntController apic_controller = {
    .reserve = apic_reserve,
    .eoi = apic_eoi,
    .set_mask = apic_set_mask,
    .priv = NULL
};
