#include "usb.h"
#include "xhci/xhci.h"
intptr_t init_usb(PciDevice* dev) {
    if(dev->prog_inferface == 0x30)
        return init_xhci(dev);
    kwarn("USB Controller Unsupported. prog_inferface=%02X", dev->prog_inferface);
    return -UNSUPPORTED;
}
