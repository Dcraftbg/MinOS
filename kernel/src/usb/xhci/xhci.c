#include "xhci.h"
#include <msi.h>
#include <iomem.h>
#include <page.h>
#include <memory.h>
#include <ptr_darray.h>
typedef struct {
    uint8_t  caplen;
    uint8_t  _reserved;
    uint16_t hci_version;
    uint32_t hcs_params1;

    uint32_t hcs_params2;
    // uint32_t ist : 4;
    // uint32_t erst_max : 4;
    // uint32_t _rsvd2 : 13;
    // uint32_t max_scratchpad_high : 5;
    // uint32_t spr : 1;
    // uint32_t max_scratchpad_low : 5;

    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hcc_params2;
} __attribute__((packed)) CapabilityRegs; 
static_assert(sizeof(CapabilityRegs) == 32, "CapabilityRegs has to be 32 bytes");
#define DEVICE_SLOTS_SHIFT     0
#define MAX_INTERRUPTERS_SHIFT 8
#define MAX_PORTS_SHIFT        24
#define DEVICE_SLOTS_MASK      (0b11111111    << DEVICE_SLOTS_SHIFT)
#define MAX_INTERRUPTERS_MASK  (0b11111111111 << MAX_INTERRUPTERS_SHIFT)
#define MAX_PORTS_MASK         (0b11111111    << MAX_PORTS_SHIFT)
static uint32_t get_max_device_slots(volatile CapabilityRegs* regs) {
    return (regs->hcs_params1 & DEVICE_SLOTS_MASK) >> DEVICE_SLOTS_SHIFT;
}
static uint32_t get_max_interrupters(volatile CapabilityRegs* regs) {
    return (regs->hcs_params1 & MAX_INTERRUPTERS_MASK) >> MAX_INTERRUPTERS_SHIFT;
}
static uint32_t get_max_ports(volatile CapabilityRegs* regs) {
    return (regs->hcs_params1 & MAX_PORTS_MASK) >> MAX_PORTS_SHIFT;
}

#define IST_SHIFT                 0
#define ERST_MAX_SHIFT            4
#define MAX_SCRATCHPAD_HIGH_SHIFT 21
#define SPR_SHIFT                 26
#define MAX_SCRATCHPAD_LOW_SHIFT  27

#define IST_MASK                 (0b1111  << IST_SHIFT)
#define ERST_MAX_MASK            (0b1111  << ERST_MAX_SHIFT)
#define MAX_SCRATCHPAD_HIGH_MASK (0b11111 << MAX_SCRATCHPAD_HIGH_SHIFT)
#define SPR_MASK                 (0b1     << SPR_SHIFT)
#define MAX_SCRATCHPAD_LOW_MASK  (0b11111 << MAX_SCRATCHPAD_LOW_SHIFT) 
static uint32_t get_ist_slots(volatile CapabilityRegs* regs) {
    return (regs->hcs_params2 & IST_MASK) >> IST_SHIFT;
}
static uint32_t get_erst_max(volatile CapabilityRegs* regs) {
    return (regs->hcs_params2 & ERST_MAX_MASK) >> ERST_MAX_SHIFT;
}
static uint32_t get_max_scratchpad_high(volatile CapabilityRegs* regs) {
    return (regs->hcs_params2 & MAX_SCRATCHPAD_HIGH_MASK) >> MAX_SCRATCHPAD_HIGH_SHIFT;
}
static uint32_t get_max_scratchpad_low(volatile CapabilityRegs* regs) {
    return (regs->hcs_params2 & MAX_SCRATCHPAD_LOW_MASK) >> MAX_SCRATCHPAD_LOW_SHIFT;
}

#define EXT_CAP_PTR_SHIFT 16
#define EXT_CAP_PTR_MASK  (0b1111111111111111 << EXT_CAP_PTR_SHIFT)

static uint32_t get_ext_cap_ptr(volatile CapabilityRegs* regs) {
    return (regs->hcc_params1 & EXT_CAP_PTR_MASK) >> EXT_CAP_PTR_SHIFT;
}

#define STATUS_CONTROL_RESET (1 << 4)
#define STATUS_CONTROL_RESET_CHANGED (1 << 21)
#define STATUS_CONTROL_POWER (1 << 9)
#define STATUS_CONTROL_CONNECT_STATUS_CHANGE (1 << 17)
typedef struct {
    uint32_t status_control;
    uint32_t power_status_cont;
    uint32_t link_info;
    uint32_t hardware_lpm;
} __attribute__((packed)) PortRegisterSet;
static_assert(sizeof(PortRegisterSet) == 4 * sizeof(uint32_t), "Update PortRegisterSet");

#define USBCMD_RUN        (1 << 0)
#define USBCMD_HCRST      (1 << 1)
#define USBCMD_INT_ENABLE (1 << 2)

#define USBSTATUS_CNR (1<<11)
typedef struct {
    uint32_t usb_cmd;
    uint32_t usb_status;
    uint32_t page_size;
    uint64_t _reserved;
    uint32_t dnctrl;
    uint64_t crcr;
    uint64_t _reserved2[2];
    uint64_t dcbaap;
    uint32_t config;
    uint32_t _reserved3[241];
    PortRegisterSet port_regs[];
} __attribute__((packed)) OperationalRegs;
#define IMAN_PENDING 0b1
#define IMAN_ENABLED 0b10
#define EVENT_HANDLER_BUSY (1 << 3)
typedef struct {
    uint32_t iman;
    // uint16_t imi;
    // uint16_t idc;
    uint32_t imod;
    uint32_t erst_size;
    uint32_t _rsvd2;
    uint64_t erst_addr;
    // Also includes DESi and EHB in low bits
    uint64_t event_ring_dequeue_ptr;
} __attribute__((packed)) ISREntry;
static_assert(sizeof(ISREntry) == 32, "ISREntry must be 32 bytes");
typedef struct {
    uint32_t mfindex;
    uint8_t _reserved[28];
    ISREntry irs[];
} __attribute__((packed)) RuntimeRegs;

enum {
    TRB_TYPE_NORMAL=1,
    TRB_TYPE_SETUP_STAGE,
    TRB_TYPE_DATA_STAGE,
    TRB_TYPE_STATUS_STAGE,
    TRB_TYPE_ISOCH,
    TRB_TYPE_LINK,
    TRB_TYPE_EVENT_DATA,
    TRB_TYPE_NOOP,
    // NOTE: If you're confused:
    // Checkout table on page 512 of the xhci spec
    TRB_TYPE_ENABLE_SLOT_CMD,
    TRB_TYPE_DISABLE_SLOT_CMD,
    TRB_TYPE_ADDRESS_DEVICE_CMD,
    TRB_TYPE_CONFIGURE_ENDPOINT_CMD,
    TRB_TYPE_EVAL_CONTEXT_CMD,
    TRB_TYPE_RESET_ENDPOINT_CMD,
    TRB_TYPE_STOP_ENDPOINT_CMD,
    TRB_TYPE_SET_TR_DEQUEUE_CMD,
    TRB_TYPE_RESET_DEVICE_CMD,
    // @optional
    TRB_TYPE_FORCE_EVENT_CMD,
    TRB_TYPE_NEG_BANDWIDTH_CMD,
    // @optional
    TRB_TYPE_SET_LATENCY_TOL_VALUE_CMD,
    // @optional
    TRB_TYPE_GET_PORT_BANDWIDTH_CMD,
    TRB_TYPE_FORCE_HEADER_CMD,
    TRB_TYPE_NOOP_CMD,
    // @optional
    TRB_TYPE_GET_EXT_PROPERTY_CMD,
    // @optional
    TRB_TYPE_SET_EXT_PROPERTY_CMD,
    TRB_TYPE_TRANSFER_EVENT=32,
    TRB_TYPE_CMD_COMPLETE_EVENT,
    TRB_TYPE_PORT_STATUS_CHANGE_EVENT,
    TRB_TYPE_BANDWIDTH_REQUETS_EVENT,
    // @optional
    TRB_TYPE_DOORBELL_EVENT,
    TRB_TYPE_HOST_CONTROLLER_EVENT,
    TRB_TYPE_DEVICE_NOTIF_EVENT,
    TRB_TYPE_MFINDEX_WRAP_EVENT,

    TRB_TYPE_COUNT
};
static const char* trb_type_str_map[TRB_TYPE_COUNT] = {
    "Undefined",
    [TRB_TYPE_NORMAL]                    = "NORMAL",
    [TRB_TYPE_SETUP_STAGE]               = "SETUP_STAGE",
    [TRB_TYPE_DATA_STAGE]                = "DATA_STAGE",
    [TRB_TYPE_STATUS_STAGE]              = "STATUS_STAGE",
    [TRB_TYPE_ISOCH]                     = "ISOCH",
    [TRB_TYPE_LINK]                      = "LINK",
    [TRB_TYPE_EVENT_DATA]                = "EVENT_DATA",
    [TRB_TYPE_NOOP]                      = "NOOP",
    [TRB_TYPE_ENABLE_SLOT_CMD]           = "ENABLE_SLOT_CMD",
    [TRB_TYPE_DISABLE_SLOT_CMD]          = "DISABLE_SLOT_CMD",
    [TRB_TYPE_ADDRESS_DEVICE_CMD]        = "ADDRESS_DEVICE_CMD",
    [TRB_TYPE_CONFIGURE_ENDPOINT_CMD]    = "CONFIGURE_ENDPOINT_CMD",
    [TRB_TYPE_EVAL_CONTEXT_CMD]          = "EVAL_CONTEXT_CMD",
    [TRB_TYPE_RESET_ENDPOINT_CMD]        = "RESET_ENDPOINT_CMD",
    [TRB_TYPE_STOP_ENDPOINT_CMD]         = "STOP_ENDPOINT_CMD",
    [TRB_TYPE_SET_TR_DEQUEUE_CMD]        = "SET_TR_DEQUEUE_CMD",
    [TRB_TYPE_RESET_DEVICE_CMD]          = "RESET_DEVICE_CMD",
    [TRB_TYPE_FORCE_EVENT_CMD]           = "FORCE_EVENT_CMD",
    [TRB_TYPE_NEG_BANDWIDTH_CMD]         = "NEG_BANDWIDTH_CMD",
    [TRB_TYPE_SET_LATENCY_TOL_VALUE_CMD] = "SET_LATENCY_TOL_VALUE_CMD",
    [TRB_TYPE_GET_PORT_BANDWIDTH_CMD]    = "GET_PORT_BANDWIDTH_CMD",
    [TRB_TYPE_FORCE_HEADER_CMD]          = "FORCE_HEADER_CMD",
    [TRB_TYPE_NOOP_CMD]                  = "NOOP_CMD",
    [TRB_TYPE_GET_EXT_PROPERTY_CMD]      = "GET_EXT_PROPERTY_CMD",
    [TRB_TYPE_SET_EXT_PROPERTY_CMD]      = "SET_EXT_PROPERTY_CMD",

    [TRB_TYPE_TRANSFER_EVENT]            = "TRANSFER_EVENT",
    [TRB_TYPE_CMD_COMPLETE_EVENT]        = "CMD_COMPLETE_EVENT",
    [TRB_TYPE_PORT_STATUS_CHANGE_EVENT]  = "PORT_STATUS_CHANGE_EVENT",
    [TRB_TYPE_BANDWIDTH_REQUETS_EVENT]   = "BANDWIDTH_REQUETS_EVENT",
    [TRB_TYPE_DOORBELL_EVENT]            = "DOORBELL_EVENT",
    [TRB_TYPE_HOST_CONTROLLER_EVENT]     = "HOST_CONTROLLER_EVENT",
    [TRB_TYPE_DEVICE_NOTIF_EVENT]        = "DEVICE_NOTIF_EVENT",
    [TRB_TYPE_MFINDEX_WRAP_EVENT]        = "MFINDEX_WRAP_EVENT",
};
static const char* trb_type_str(uint8_t type) {
    if(type >= TRB_TYPE_COUNT) return "Undefined";
    return trb_type_str_map[type];
}
typedef struct {
    uint64_t data;
    uint16_t trb_length;
    uint16_t td_size : 4;
    uint16_t interrupt_target : 12;
    uint16_t cycle : 1;
    uint16_t eval_next : 1;
    uint16_t isp : 1;
    uint16_t ns : 1;
    uint16_t chain : 1;
    uint16_t ioc : 1;
    uint16_t immediate : 1;
    uint16_t _rsvd2 : 2;
    uint16_t bei : 1;
    uint16_t type : 6;
    uint16_t _rsvd1;
} __attribute__((packed)) TRB;
static_assert(sizeof(TRB) == 4*sizeof(uint32_t), "TRB must be 4 registers wide");
typedef struct {
    uint64_t addr;
    // FIXME: Make 32 bit
    uint16_t size;
    uint16_t _rsvd1;
    uint32_t _rsvd2;
} __attribute__((packed)) ERSTEntry;
static_assert(sizeof(ERSTEntry) == 4*sizeof(uint32_t), "ERSTEntry must be 4 registers wide");
typedef struct {
    uint32_t id_and_next;
    // uint8_t _id;
    // uint8_t _next_in_dwords;
} __attribute__((packed)) ExtCapEntry;
static_assert(sizeof(ExtCapEntry) == 1*sizeof(uint32_t), "ExtCapEntry must be 1 register wide");
typedef struct {
    uint32_t header;
} __attribute__((packed)) USBLegacySupportCap;
uint32_t usb_legacy_support_get_bios(const volatile USBLegacySupportCap* cap) {
    return cap->header & (1 << 16); 
}
void usb_legacy_support_set_os(volatile USBLegacySupportCap* cap) {
    cap->header = cap->header | (1 << 24); 
}

typedef struct {
    // [ Cap ID ] [ Next Cap ] [ Revision Minor ] [Revision Major]
    uint32_t header;
    uint32_t name;
    uint32_t header2;
    uint32_t slot_type;
    uint32_t port_support[];
} __attribute__((packed)) SupportProtCap;
uint8_t supported_prot_cap_revision_minor(const volatile SupportProtCap* cap) {
    return (cap->header >> 16) & 0xFF;
}
uint8_t supported_prot_cap_revision_major(const volatile SupportProtCap* cap) {
    return (cap->header >> 24) & 0xFF;
}
uint8_t supported_prot_cop_port_offset(const volatile SupportProtCap* cap) {
    return (cap->header2 >> 0) & 0xFF;
}
uint8_t supported_prot_cop_port_count(const volatile SupportProtCap* cap) {
    return (cap->header2 >> 8) & 0xFF;
}

enum {
    EXT_CAP_ID_USB_LEGACY_SUPPORT=1,
    EXT_CAP_ID_SUPPORT_PROTOCOL,
};
uint8_t get_id(const volatile ExtCapEntry* entry) {
    return entry->id_and_next & 0xFF;
}
uint8_t get_next_in_dwords(const volatile ExtCapEntry* entry) {
    return (entry->id_and_next >> 8) & 0xFF;
}

typedef struct XhciController XhciController;
typedef PtrDarray XhciControllers;
static XhciControllers xhci_controllers = {0};
static Cache* xhci_controller_cache=NULL;

static inline bool xhci_controllers_add(XhciControllers* da, XhciController* controller) {
    if(!ptr_darray_reserve(da, 1)) return false;
    da->items[da->len++] = controller;
    return true;
}
static inline XhciController* xhci_controllers_pop(XhciControllers* da) {
    assert(da->len);
    return da->items[--da->len];
}
typedef struct {
    uint8_t revision_major, revision_minor, slot_id;
} Port;

struct XhciController {
    volatile CapabilityRegs* capregs;
    Port* ports_items;
    size_t ports_count;
    size_t mmio_len;
    size_t dcbaa_len;
    paddr_t   dcbaa_phys;
    uint64_t* dcbaa;
    paddr_t cmd_ring_phys;
    TRB* cmd_ring;
    size_t cmd_ring_doorbell;
    bool cmd_ring_cycle;
    // For ISR[0]. The only one supported currently
    size_t erst_len;
    paddr_t erst_phys;
    ERSTEntry* erst;
    // For ISR[0].erst[0]
    paddr_t event_ring_phys;
    TRB* event_ring;
    bool event_ring_cycle;
    // TODO: Maybe keep track of the scratchpad buffers to deallocate ourselves
    // For Scratchpad
    size_t scratchpad_len;
    paddr_t scratchpad_phys;
    uint64_t* scratchpad;
};
#define xhci_dcbaa_pages(cont) (((cont->dcbaa_len*sizeof(uint64_t))+(PAGE_SIZE-1))/PAGE_SIZE)
#define xhci_erst_pages(cont) ((cont->erst_len*sizeof(ERSTEntry) + (PAGE_SIZE-1)) / PAGE_SIZE)
static inline volatile uint32_t* xhci_doorbells(XhciController* c) {
    return ((void*)c->capregs) + (c->capregs->dboff);
}
static inline volatile RuntimeRegs* xhci_runtime_regs(XhciController* c) {
    return ((void*)c->capregs) + (c->capregs->rtsoff);
}
static inline volatile OperationalRegs* xhci_op_regs(XhciController* c) {
    return ((void*)c->capregs) + (c->capregs->caplen);
}
#define CMD_RING_CAP 256
#define EVENT_RING_CAP 256
// TODO: Handle multiple devices
// TODO: Allow commiting multiple commands / trb_head accepting index of command (which wraps around)
static volatile TRB* xhci_trb_head(XhciController* c) {
    return &c->cmd_ring[c->cmd_ring_doorbell];
}
static void xhci_trb_commit(XhciController* cont) {
    cont->cmd_ring_doorbell += 1;
    if(cont->cmd_ring_doorbell >= (CMD_RING_CAP-1)) {
        TRB* link = &cont->cmd_ring[CMD_RING_CAP-1];
        link->cycle = cont->cmd_ring_cycle;
        // NOTE: Toggle Cycle bit
        link->eval_next = 1;
        cont->cmd_ring_cycle = !cont->cmd_ring_cycle;
        cont->cmd_ring_doorbell = 0;
    }
    xhci_doorbells(cont)[0] = 0;
}

size_t n = 0;
void xhci_handler(PciDevice* dev) {
    XhciController* cont = dev->priv;
    volatile ISREntry* irs = &xhci_runtime_regs(cont)->irs[0];
    size_t i = (((irs->event_ring_dequeue_ptr) - cont->event_ring_phys) / sizeof(TRB));
    // kinfo("%zu> Called xhci_handler %p %d", n++, dev, cont->event_ring[i].cycle);
    while(cont->event_ring[i].cycle == cont->event_ring_cycle) {
        kinfo("%zu> Event %s", n++, trb_type_str(cont->event_ring[i].type));
        i++;
        if(i >= EVENT_RING_CAP) {
            cont->event_ring_cycle = !cont->event_ring_cycle;
            i = 0;
        }
    }
    irs->event_ring_dequeue_ptr = (cont->event_ring_phys + (i % EVENT_RING_CAP) * sizeof(TRB)) | EVENT_HANDLER_BUSY; 
    irs->iman = irs->iman | IMAN_PENDING | IMAN_ENABLED;
    irq_eoi(dev->irq);
}
static intptr_t init_cmd_ring(XhciController* cont) {
    cont->cmd_ring_phys = kernel_pages_alloc((CMD_RING_CAP*sizeof(TRB) + (PAGE_SIZE-1)) / PAGE_SIZE);
    if(!cont->cmd_ring_phys) return -NOT_ENOUGH_MEM;
    cont->cmd_ring = iomap_bytes(cont->cmd_ring_phys, CMD_RING_CAP*sizeof(TRB), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->cmd_ring) return -NOT_ENOUGH_MEM;
    memset(cont->cmd_ring, 0, CMD_RING_CAP*sizeof(TRB));
    TRB* last = &cont->cmd_ring[CMD_RING_CAP-1];
    last->cycle = 1;
    last->data = cont->cmd_ring_phys+0*sizeof(TRB);
    last->type = TRB_TYPE_LINK;
    xhci_op_regs(cont)->crcr = cont->cmd_ring_phys | 0b1;
    cont->cmd_ring_cycle = 1;
    return 0;
}
static void deinit_cmd_ring(XhciController* cont) {
    if(cont->cmd_ring) iounmap_bytes(cont->cmd_ring, CMD_RING_CAP*sizeof(TRB));
    if(cont->cmd_ring_phys) kernel_pages_dealloc(cont->cmd_ring_phys, CMD_RING_CAP*sizeof(TRB));
}
static intptr_t init_dcbaa(XhciController* cont) {
    cont->dcbaa_len = get_max_device_slots(cont->capregs);
    // Set max device slots to dcbaa_len
    xhci_op_regs(cont)->config = (xhci_op_regs(cont)->config & ~0xFF) | cont->dcbaa_len;
    cont->dcbaa_phys = kernel_pages_alloc(xhci_dcbaa_pages(cont));
    if(!cont->dcbaa_phys) return -NOT_ENOUGH_MEM;
    cont->dcbaa = iomap(cont->dcbaa_phys, xhci_dcbaa_pages(cont), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->dcbaa) return -NOT_ENOUGH_MEM;
    memset(cont->dcbaa, 0, cont->dcbaa_len * sizeof(cont->dcbaa[0]));
    xhci_op_regs(cont)->dcbaap = cont->dcbaa_phys;
    return 0;
}
static void deinit_dcbaa(XhciController* cont) {
    if(cont->dcbaa) iounmap(cont->dcbaa, xhci_dcbaa_pages(cont));
    if(cont->dcbaa_phys) kernel_pages_dealloc(cont->dcbaa_phys, xhci_dcbaa_pages(cont));
}

static intptr_t init_erst(XhciController* cont) {
    cont->erst_len = 1;
    cont->erst_phys = kernel_pages_alloc(xhci_erst_pages(cont));
    if(!cont->erst_phys) return -NOT_ENOUGH_MEM;
    cont->erst = iomap(cont->erst_phys, xhci_erst_pages(cont), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->erst) return -NOT_ENOUGH_MEM;
    memset(cont->erst, 0, cont->erst_len * sizeof(cont->erst[0]));
    return 0;
}
static void deinit_erst(XhciController* cont) {
    size_t pages = xhci_erst_pages(cont);
    if(cont->erst) iounmap(cont->erst, pages);
    if(cont->erst_phys) kernel_pages_dealloc(cont->erst_phys, pages);
}

static intptr_t init_event_ring(XhciController* cont) {
    cont->event_ring_phys = kernel_pages_alloc((EVENT_RING_CAP*sizeof(TRB) + (PAGE_SIZE-1)) / PAGE_SIZE);
    if(!cont->event_ring_phys) return -NOT_ENOUGH_MEM;
    cont->event_ring = iomap_bytes(cont->event_ring_phys, EVENT_RING_CAP*sizeof(TRB), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->event_ring) return -NOT_ENOUGH_MEM;
    memset(cont->event_ring, 0, EVENT_RING_CAP*sizeof(TRB));
    cont->event_ring_cycle = 1;
    // NOTE: This maybe shouldn't be done by us
    // TRB* last = &cont->event_ring[EVENT_RING_CAP-1];
    // last->cycle = 1;
    // last->data = cont->event_ring_phys+0*sizeof(TRB);
    // last->type = TRB_TYPE_LINK;
    return 0;
}
static void deinit_event_ring(XhciController* cont) {
    if(cont->event_ring) iounmap_bytes(cont->event_ring, EVENT_RING_CAP*sizeof(TRB));
    if(cont->event_ring_phys) kernel_pages_dealloc(cont->event_ring_phys, EVENT_RING_CAP*sizeof(TRB));
}
static size_t xhci_pages_native(XhciController* cont) {
    static_assert(PAGE_SIZE == 4096, "Update xhci_pages_native");
    uint16_t page_size = xhci_op_regs(cont)->page_size;
    for(size_t i = 0; i < 16; ++i) {
        if (page_size & (1 << i))
            return 1 << i;
    }
    return 0;
}
static uint32_t xhci_max_scratchpad(XhciController* cont) {
    return (get_max_scratchpad_high(cont->capregs) << 5) | (get_max_scratchpad_low(cont->capregs));
}
static intptr_t init_scratchpad(XhciController* cont) {
    cont->scratchpad_len = xhci_max_scratchpad(cont);
    // NOTE: The controller doesn't require any scratchpad buffers
    if(cont->scratchpad_len == 0) return 0;
    if(cont->dcbaa_len == 0) return -SIZE_MISMATCH;
    size_t scratchpad_pages = (cont->scratchpad_len * sizeof(uint64_t) + (PAGE_SIZE-1)) / PAGE_SIZE;
    cont->scratchpad_phys = kernel_pages_alloc(scratchpad_pages);
    if(!cont->scratchpad_phys) return -NOT_ENOUGH_MEM;
    cont->scratchpad = iomap(cont->scratchpad_phys, scratchpad_pages, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->scratchpad) return -NOT_ENOUGH_MEM;
    memset(cont->scratchpad, 0, cont->scratchpad_len * sizeof(uint64_t));

    size_t pages = xhci_pages_native(cont);
    if(!pages) return -INVALID_TYPE;
    for(size_t i = 0; i < cont->scratchpad_len; ++i) {
        cont->scratchpad[i] = kernel_pages_alloc(pages);
        if(!cont->scratchpad[i]) return -NOT_ENOUGH_MEM;
    }
    cont->dcbaa[0] = cont->scratchpad_phys;
    return 0;
}
static void deinit_scratchpad(XhciController* cont) {
    size_t scratchpad_pages = (cont->scratchpad_len * sizeof(uint64_t) + (PAGE_SIZE-1)) / PAGE_SIZE;
    if(cont->scratchpad) {
        for(size_t i = 0; i < cont->scratchpad_len; ++i) {
            if(cont->scratchpad[i]) kernel_pages_alloc(cont->scratchpad[i]);
        }
        iounmap(cont->scratchpad, scratchpad_pages);
    }
    if(cont->scratchpad_phys) kernel_pages_dealloc(cont->scratchpad_phys, scratchpad_pages);
}
static intptr_t xhci_bound_check(PciDevice* dev, XhciController* cont) {
    if(dev->bar0.size < sizeof(CapabilityRegs)) {
        kerror("Bar0 too small for capregs");
        return -UNSUPPORTED;
    }
    cont->capregs = dev->bar0.as.mmio.addr;
    if(cont->capregs->caplen < sizeof(CapabilityRegs)) {
        kerror("Caplen must be at a minimum %zu in size (actual = %zu)", sizeof(CapabilityRegs), cont->capregs->caplen);
        return -UNSUPPORTED;
    }
    if(dev->bar0.size < cont->capregs->caplen) {
        kerror("Bar0 too small for capregs");
        return -UNSUPPORTED;
    }
    if(dev->bar0.size < cont->capregs->caplen + 0xBFF) {
        kerror("Bar0 too small for capregs + opregs");
        return -UNSUPPORTED;
    }
    if(dev->bar0.size < cont->capregs->rtsoff + sizeof(RuntimeRegs) + sizeof(ISREntry) * get_max_interrupters(cont->capregs)) {
        kerror("rtsoff out of bounds");
        return -UNSUPPORTED;
    }
    if(dev->bar0.size < cont->capregs->dboff + sizeof(uint32_t)*256) {
        kerror("dboff out of bounds");
        return -UNSUPPORTED;
    }
    return 0;
}
static intptr_t take_ownership(XhciController* cont) {
    uint32_t ptr = get_ext_cap_ptr(cont->capregs) * sizeof(uint32_t);
    if(!ptr) return 0;
    for(;;) {
        if(ptr + sizeof(ExtCapEntry) >= cont->mmio_len) {
            kerror("Invalid extended capability offset");
            return -INVALID_OFFSET;
        }
        volatile ExtCapEntry* cap_entry = (void*)(((uint8_t*)cont->capregs) + ptr);
        uint8_t next_in_dwords = get_next_in_dwords(cap_entry);
        uint8_t id = get_id(cap_entry);
        if (id == EXT_CAP_ID_USB_LEGACY_SUPPORT) {
            volatile USBLegacySupportCap* cap = (volatile USBLegacySupportCap*)cap_entry;
            if(ptr + sizeof(USBLegacySupportCap) >= cont->mmio_len) {
                kerror("Invalid legacy support capability offset");
                return -INVALID_OFFSET;
            }
            if(!usb_legacy_support_get_bios(cap)) {
                kinfo("BIOS doesn't work this");
                return 0;
            }
            usb_legacy_support_set_os(cap);
            while(usb_legacy_support_get_bios(cap)) {
                kinfo("Still bios");
            }
            return 0;
        }
        if(next_in_dwords == 0) break;
        ptr += next_in_dwords * sizeof(uint32_t);
    }
    return 0;
}

static intptr_t enum_ext_cap(XhciController* cont) {
    uint32_t ptr = get_ext_cap_ptr(cont->capregs) * sizeof(uint32_t);
    if(!ptr) return 0;
    for(;;) {
        if(ptr + sizeof(ExtCapEntry) >= cont->mmio_len) {
            kerror("Invalid extended capability offset");
            return -INVALID_OFFSET;
        }
        volatile ExtCapEntry* cap_entry = (void*)(((uint8_t*)cont->capregs) + ptr);
        uint8_t next_in_dwords = get_next_in_dwords(cap_entry);
        uint8_t id = get_id(cap_entry);
        kinfo("Extended Capability with id %d", id);
        switch(id) {
        case EXT_CAP_ID_SUPPORT_PROTOCOL: {
            volatile SupportProtCap* cap = (volatile SupportProtCap*)cap_entry;
            if(ptr + sizeof(SupportProtCap) >= cont->mmio_len) {
                kerror("Invalid supported capability offset");
                return -INVALID_OFFSET;
            }
            uint8_t revision_major = supported_prot_cap_revision_major(cap), revision_minor = supported_prot_cap_revision_minor(cap);
            size_t ports_count = supported_prot_cop_port_count(cap), ports_offset = supported_prot_cop_port_offset(cap);
            if(ports_count <= 0 || ports_offset <= 0) {
                kerror("Invalid values for count = %zu offset = %zu", ports_count, ports_offset);
                return -INVALID_OFFSET;
            }
            ports_offset--;
            if(ports_offset + ports_count > cont->ports_count) {
                kerror("Out of range");
                return -INVALID_OFFSET;
            }
            char name[10] = {0};
            *((uint32_t*)name) = cap->name;
            kinfo("SupportProtCap name: %s %d.%d port off %zu count %zu", name, revision_major, revision_minor, ports_offset, ports_count);
            for(size_t i = 0; i < ports_count; ++i) {
                Port* port = &cont->ports_items[i + ports_offset];
                port->revision_major = revision_major;
                port->revision_minor = revision_minor;
            }
        } break;
        }
        if(next_in_dwords == 0) break;
        ptr += next_in_dwords * sizeof(uint32_t);
    }
    return 0;
}
intptr_t init_xhci(PciDevice* dev) {
    intptr_t e;
    kinfo("BAR:");
    if(!dev->bar0.mmio) {
        kerror("usb xHCI may not be non-mmio");
        return -INVALID_TYPE;
    }
    pci_device_enum_caps(dev);
    if(!dev->msi_offset && !dev->msi_x_offset) {
        kerror("usb xHCI doesn't support MSI(-X)");
        return -INVALID_TYPE;
    }
    // FIXME: Remove this and add support for MSI-X
    if(!dev->msi_offset) {
        kerror("usb xHCI driver currently only supports MSI");
        return -INVALID_TYPE;
    }
    kinfo("msi_offset=0x%02X", dev->msi_offset);
    kinfo("(MMIO) %p %zu bytes", dev->bar0.as.mmio.addr, dev->bar0.size);
    if(!xhci_controller_cache && !(xhci_controller_cache = create_new_cache(sizeof(XhciController), "XhciController")))
        return -NOT_ENOUGH_MEM;
    XhciController* cont = cache_alloc(xhci_controller_cache);
    // TODO: Maybe cleanup cont cache?
    if(!cont) return -NOT_ENOUGH_MEM;
    memset(cont, 0, sizeof(*cont));
    if(!xhci_controllers_add(&xhci_controllers, cont)) {
        e = -NOT_ENOUGH_MEM;
        goto cont_add_err;
    }
    if((e=xhci_bound_check(dev, cont)) < 0) goto bound_check_err;
    cont->mmio_len = dev->bar0.size;
    take_ownership(cont);
    kinfo("max_device_slots: %zu", get_max_device_slots(cont->capregs));
    kinfo("max_interrupters: %zu", get_max_interrupters(cont->capregs));
    kinfo("max_ports: %zu", cont->ports_count = get_max_ports(cont->capregs));
    kinfo("page size: %zu", xhci_pages_native(cont) << PAGE_SHIFT);
    kinfo("pci->command %04X", dev->command);
    while(xhci_op_regs(cont)->usb_status & USBSTATUS_CNR);
    cont->ports_items = kernel_malloc(cont->ports_count * sizeof(Port));
    if(!cont->ports_items) goto ports_items_err;
    memset(cont->ports_items, 0, cont->ports_count * sizeof(*cont->ports_items));
    kinfo("Ready");
    xhci_op_regs(cont)->usb_cmd = xhci_op_regs(cont)->usb_cmd | USBCMD_HCRST;
    while(xhci_op_regs(cont)->usb_cmd & USBCMD_HCRST);
    kinfo("Reset complete");
    if((e=init_dcbaa(cont)) < 0) goto dcbaa_err;
    if((e=init_cmd_ring(cont)) < 0) goto cmd_ring_err;
    // TODO: Technically incorrect. Its ERST per interrupter. But there's only 1 currently
    if((e=init_erst(cont)) < 0) goto erst_err;
    if((e=init_event_ring(cont)) < 0) goto event_ring_err;
    if((e=init_scratchpad(cont)) < 0) goto scratchpad_err;
    if((e=enum_ext_cap(cont)) < 0) goto enum_ext_cap_err;


    cont->erst[0].addr = cont->event_ring_phys;
    cont->erst[0].size = EVENT_RING_CAP;
    volatile ISREntry* irs = &xhci_runtime_regs(cont)->irs[0];
    irs->erst_size = (irs->erst_size & ~0xFFFF) | 1;
    irs->erst_addr = cont->erst_phys;
    irs->event_ring_dequeue_ptr = cont->event_ring_phys | EVENT_HANDLER_BUSY;
    irs->iman = irs->iman /*| IMAN_PENDING*/ | IMAN_ENABLED;
    xhci_op_regs(cont)->usb_cmd = xhci_op_regs(cont)->usb_cmd | USBCMD_INT_ENABLE;

    dev->handler = xhci_handler; 
    dev->priv = cont;
    msi_register(&msi_manager, dev);
    // irq_clear(dev->irq);
    kinfo("xHCI Running");
    xhci_op_regs(cont)->usb_cmd = xhci_op_regs(cont)->usb_cmd | USBCMD_RUN;
    for(size_t i = 0; i < cont->ports_count; ++i) {
        volatile PortRegisterSet* reg = &xhci_op_regs(cont)->port_regs[i];
        if(!(reg->status_control & STATUS_CONTROL_POWER)) {
            kinfo("Skipping %zu", i);
            continue;
        }
        reg->status_control = /* STATUS_CONTROL_RESET_CHANGED | */ STATUS_CONTROL_POWER | STATUS_CONTROL_CONNECT_STATUS_CHANGE;
    }
    // NOTE: FLADJ is not set. Done by the BIOS? Could be an issue in the future.
    return 0;
enum_ext_cap_err:
scratchpad_err:
    deinit_scratchpad(cont);
event_ring_err:
    deinit_event_ring(cont);
erst_err:
    deinit_erst(cont);
cmd_ring_err:
    deinit_cmd_ring(cont);
dcbaa_err:
    deinit_dcbaa(cont);
    kernel_dealloc(cont->ports_items, cont->ports_count * sizeof(*cont->ports_items));
ports_items_err:
bound_check_err:
    xhci_controllers_pop(&xhci_controllers);
cont_add_err:
    cache_dealloc(xhci_controller_cache, cont);
    return e;
}
