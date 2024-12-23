#include "xhci.h"

intptr_t init_xhci(PciDevice* dev) {
    // NOTE: FLADJ is not set. Done by the BIOS? Could be an issue in the future.
    kinfo("BAR:");
    if(!dev->bar0.mmio) {
        kerror("usb xHCI may not be non-mmio");
        return -INVALID_TYPE;
    }
    kinfo("(MMIO) %p %zu bytes", dev->bar0.as.mmio.addr, dev->bar0.size);
    return 0;
}
