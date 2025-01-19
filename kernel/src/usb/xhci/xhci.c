#include "xhci.h"
#include <msi.h>
#include <iomem.h>
#include <page.h>
#include <memory.h>
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
    uint32_t crcr;
    uint64_t _reserved2[2];
    uint64_t dcbaap;
    uint32_t config;
} __attribute__((packed)) OperationalRegs;
#define IMAN_PENDING 0b1
#define IMAN_ENABLED 0b10
typedef struct {
    uint32_t iman;
    uint16_t imi;
    uint16_t idc;
    uint16_t erst_size;
    uint16_t _rsvd1;
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
    TRB_TYPE_ENABLE_SLOT_CMD,
    TRB_TYPE_DISABLE_SLOT_CMD,
    TRB_TYPE_ADDRESS_DEVICE_CMD,
};
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
    uint16_t type : 5;
    uint16_t _rsvd1;
} __attribute__((packed)) TRB;
static_assert(sizeof(TRB) == 4*sizeof(uint32_t), "TRB must be 4 registers wide");
typedef struct {
    uint64_t addr;
    uint16_t size;
    uint16_t _rsvd1;
    uint32_t _rsvd2;
} __attribute__((packed)) ERSTEntry;
static_assert(sizeof(ERSTEntry) == 4*sizeof(uint32_t), "ERSTEntry must be 4 registrers wide");

typedef struct XhciController XhciController;
typedef struct {
    XhciController** data;
    size_t len, cap;
} XhciControllers;
XhciControllers xhci_controllers = {0};
static Cache* xhci_controller_cache=NULL;

bool xhci_controllers_reserve(XhciControllers* da, size_t extra) {
    if(da->len + extra > da->cap) {
        size_t new_cap = da->cap*2 + extra;
        void* new_data = kernel_malloc(new_cap * sizeof(da->data[0]));
        if(!new_data) return false;
        kernel_dealloc(da->data, da->cap * sizeof(da->data[0]));
        da->cap = new_cap;
        da->data = new_data;
    }
    return true;
}
static inline bool xhci_controllers_add(XhciControllers* da, XhciController* controller) {
    if(!xhci_controllers_reserve(da, 1)) return false;
    da->data[da->len++] = controller;
    return true;
}
static inline XhciController* xhci_controllers_pop(XhciControllers* da) {
    assert(da->len);
    return da->data[--da->len];
}

struct XhciController {
    volatile CapabilityRegs* capregs;
    size_t dcbaa_len;
    paddr_t   dcbaa_phys;
    uint64_t* dcbaa;
    paddr_t cmd_ring_phys;
    TRB* cmd_ring;
    // For ISR[0]. The only one supported currently
    size_t erst_len;
    paddr_t erst_phys;
    ERSTEntry* erst;
    // For ISR[0].erst[0]
    paddr_t event_ring_phys;
    TRB* event_ring;
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



void xhci_handler(PciDevice* dev) {
    kinfo("Called xhci_handler %p", dev);
    XhciController* cont = dev->priv;
    volatile ISREntry* irs = &xhci_runtime_regs(cont)->irs[0];
    irs->iman |= IMAN_PENDING | IMAN_ENABLED;
    irs->event_ring_dequeue_ptr = (irs->event_ring_dequeue_ptr + sizeof(TRB)) | 0b1000;
    irq_eoi(dev->irq);
}
#define CMD_RING_CAP 256
static intptr_t init_cmd_ring(XhciController* cont) {
    paddr_t phys = kernel_pages_alloc((CMD_RING_CAP*sizeof(TRB) + (PAGE_SIZE-1)) / PAGE_SIZE);
    if(!phys) return -NOT_ENOUGH_MEM;
    cont->cmd_ring = iomap_bytes(phys, CMD_RING_CAP*sizeof(TRB), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->cmd_ring) return -NOT_ENOUGH_MEM;
    TRB* last = &cont->cmd_ring[CMD_RING_CAP-1];
    last->cycle = 1;
    last->data = phys+0*sizeof(TRB);
    last->type = TRB_TYPE_LINK;
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

#define EVENT_RING_CAP 256
static intptr_t init_event_ring(XhciController* cont) {
    cont->event_ring_phys = kernel_pages_alloc((EVENT_RING_CAP*sizeof(TRB) + (PAGE_SIZE-1)) / PAGE_SIZE);
    if(!cont->event_ring_phys) return -NOT_ENOUGH_MEM;
    cont->event_ring = iomap_bytes(cont->event_ring_phys, EVENT_RING_CAP*sizeof(TRB), KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_CACHE_DISABLE);
    if(!cont->event_ring) return -NOT_ENOUGH_MEM;
    memset(cont->event_ring, 0, EVENT_RING_CAP*sizeof(TRB));
    TRB* last = &cont->event_ring[EVENT_RING_CAP-1];
    last->cycle = 1;
    last->data = cont->event_ring_phys+0*sizeof(TRB);
    last->type = TRB_TYPE_LINK;
    return 0;
}
static void deinit_event_ring(XhciController* cont) {
    if(cont->event_ring) iounmap_bytes(cont->event_ring, EVENT_RING_CAP*sizeof(TRB));
    if(cont->event_ring_phys) kernel_pages_dealloc(cont->event_ring_phys, EVENT_RING_CAP*sizeof(TRB));
}
static size_t xhci_pages_native(XhciController* cont) {
    return 1 << (xhci_op_regs(cont)->page_size + (12-PAGE_SHIFT));
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
    uint32_t reg1      = pci_config_read_dword(dev->bus, dev->slot, dev->func, dev->msi_offset + 0x4);
    uint32_t bir       = reg1 & 0b111;
    uint32_t table_off = reg1 & (~0b111);
    if(bir) {
        kerror("usb xHCI driver currently only supports BIR=0. BIR=%d", bir);
        return -INVALID_TYPE;
    }
    (void)table_off;
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
    kinfo("max_device_slots: %zu", get_max_device_slots(cont->capregs));
    kinfo("max_interrupters: %zu", get_max_interrupters(cont->capregs));
    kinfo("max_ports: %zu", get_max_ports(cont->capregs));
    kinfo("page size: %zu", 1<<(xhci_op_regs(cont)->page_size + 12));
    while(xhci_op_regs(cont)->usb_status & USBSTATUS_CNR);
    if((e=init_dcbaa(cont)) < 0) goto dcbaa_err;
    if((e=init_cmd_ring(cont)) < 0) goto cmd_ring_err;
    // TODO: Technically incorrect. Its ERST per interrupter. But there's only 1 currently
    if((e=init_erst(cont)) < 0) goto erst_err;
    if((e=init_event_ring(cont)) < 0) goto event_ring_err;
    if((e=init_scratchpad(cont)) < 0) goto scratchpad_err;
    cont->erst[0].addr = cont->event_ring_phys;
    cont->erst[0].size = EVENT_RING_CAP;
    volatile ISREntry* irs = &xhci_runtime_regs(cont)->irs[0];
    irs->erst_size = 1;
    irs->erst_addr = cont->erst_phys;
    irs->event_ring_dequeue_ptr = cont->event_ring_phys;
    irs->iman |= IMAN_PENDING | IMAN_ENABLED;
    xhci_op_regs(cont)->usb_cmd |= USBCMD_INT_ENABLE;

    dev->handler = xhci_handler; 
    dev->priv = cont;
    msi_register(&msi_manager, dev);
    irq_clear(dev->irq);
    kinfo("xHCI Running");
    xhci_op_regs(cont)->usb_cmd |= USBCMD_RUN;
    // NOTE: FLADJ is not set. Done by the BIOS? Could be an issue in the future.
    return 0;
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
bound_check_err:
    xhci_controllers_pop(&xhci_controllers);
cont_add_err:
    cache_dealloc(xhci_controller_cache, cont);
    return e;
}
