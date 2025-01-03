#include "xhci.h"
typedef struct {
    uint8_t  caplen;
    uint8_t  _reserved;
    uint16_t hci_version;
    uint32_t hcs_params1;
    uint32_t hcs_params2;
    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hcc_params2;
} __attribute__((packed)) CapabilityRegs; 
// -======================- hcs_params1 -=======================-
#define DEVICE_SLOTS_MASK      0b00000000000000000000000011111111
#define MAX_INTERRUPTERS_MASK  0b00000000000001111111111100000000
//      <RESERVED>             0b00000000111110000000000000000000
#define MAX_PORTS_MASK         0b11111111000000000000000000000000

#define DEVICE_SLOTS_SHIFT     0
#define MAX_INTERRUPTERS_SHIFT 8
#define MAX_PORTS_SHIFT        24
static inline uint32_t max_device_slots(CapabilityRegs* regs) {
    return (regs->hcs_params1 & DEVICE_SLOTS_MASK    ) >> DEVICE_SLOTS_SHIFT;
}
static inline uint32_t max_interrupters(CapabilityRegs* regs) {
    return (regs->hcs_params1 & MAX_INTERRUPTERS_MASK) >> MAX_INTERRUPTERS_SHIFT;
}
static inline uint32_t max_ports(CapabilityRegs* regs) {
    return (regs->hcs_params1 & MAX_PORTS_MASK       ) >> MAX_PORTS_SHIFT;
}
// -============================================================-
#define USBSTATUS_CNR (1<<11)
typedef struct {
    uint32_t usb_cmd;
    uint32_t usb_status;
    uint32_t page_size;
    uint64_t _reserved;
    uint32_t dnctrl;
    uint32_t crcr;
    uint64_t _reserved2[2];
    uint32_t dcbaap;
    uint32_t config;
} __attribute__((packed)) OperationalRegs;
typedef struct {
    uint32_t mfindex;
    uint8_t _reserved[28];
    uint32_t irs[1024];
} __attribute__((packed)) RuntimeRegs;
typedef struct {
    CapabilityRegs* capregs;
} XhciController;
static inline uint32_t* xhci_doorbells(XhciController* c) {
    return ((void*)c->capregs) + (c->capregs->dboff);
}
static inline RuntimeRegs* xhci_runtime_regs(XhciController* c) {
    return ((void*)c->capregs) + (c->capregs->rtsoff);
}
static inline OperationalRegs* xhci_op_regs(XhciController* c) {
    return ((void*)c->capregs) + (c->capregs->caplen);
}
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
intptr_t init_xhci(PciDevice* dev) {
    intptr_t e;
    kinfo("BAR:");
    if(!dev->bar0.mmio) {
        kerror("usb xHCI may not be non-mmio");
        return -INVALID_TYPE;
    }
    kinfo("(MMIO) %p %zu bytes", dev->bar0.as.mmio.addr, dev->bar0.size);
    if(!xhci_controller_cache && !(xhci_controller_cache = create_new_cache(sizeof(XhciController), "XhciController")))
        return -NOT_ENOUGH_MEM;
    XhciController* cont = cache_alloc(xhci_controller_cache);
    // TODO: Maybe cleanup cont cache?
    if(!cont) return -NOT_ENOUGH_MEM;
    if(!xhci_controllers_add(&xhci_controllers, cont)) {
        e = -NOT_ENOUGH_MEM;
        goto cont_add_err;
    }
    if(dev->bar0.size < sizeof(CapabilityRegs)) {
        kerror("Bar0 too small for capregs");
        e = -UNSUPPORTED;
        goto unsup_size_err;
    }
    cont->capregs = dev->bar0.as.mmio.addr;
    if(cont->capregs->caplen < sizeof(CapabilityRegs)) {
        kerror("Caplen must be at a minimum %zu in size (actual = %zu)", sizeof(CapabilityRegs), cont->capregs->caplen);
        e = -UNSUPPORTED;
        goto unsup_size_err;
    }
    if(dev->bar0.size < cont->capregs->caplen) {
        kerror("Bar0 too small for capregs");
        e = -UNSUPPORTED;
        goto unsup_size_err;
    }
    if(dev->bar0.size < cont->capregs->caplen + 0xBFF) {
        kerror("Bar0 too small for capregs + opregs");
        e = -UNSUPPORTED;
        goto unsup_size_err;
    }
    if(dev->bar0.size < cont->capregs->rtsoff + sizeof(RuntimeRegs)) {
        kerror("rtsoff out of bounds");
        e = -UNSUPPORTED;
        goto unsup_size_err;
    }
    if(dev->bar0.size < cont->capregs->dboff + sizeof(uint32_t)*256) {
        kerror("dboff out of bounds");
        e = -UNSUPPORTED;
        goto unsup_size_err;
    }
    kinfo("max_device_slots: %zu", max_device_slots(cont->capregs));
    kinfo("max_interrupters: %zu", max_interrupters(cont->capregs));
    kinfo("max_ports: %zu", max_ports(cont->capregs));
    volatile OperationalRegs* op = xhci_op_regs(cont);
    kinfo("page size: %zu", 1<<(op->page_size + 12));
    while(op->usb_status & USBSTATUS_CNR);
    kinfo("Ready!");
    // NOTE: FLADJ is not set. Done by the BIOS? Could be an issue in the future.
    return 0;
unsup_size_err:
    xhci_controllers_pop(&xhci_controllers);
cont_add_err:
    cache_dealloc(xhci_controller_cache, cont);
    return e;
}
