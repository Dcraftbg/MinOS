#include "msi.h"
#include <log.h>
#include <interrupt.h>
#include <minos/status.h>
static intptr_t msi_reserve_irq(MSIManager* m) {
    for(size_t i = 0; i < sizeof(m->irqs); ++i) {
        if(m->irqs[i] == 0xFF) continue;
        size_t j = 0;
        while(m->irqs[i] & (1 << j)) j++;
        m->irqs[i] |= 1<<j;
        return (i * 8 + j);
    }
    return -LIMITS;
}

extern IrqHandler msi_dispatch;
intptr_t msi_register(MSIManager* m, PciDevice* dev) {
    intptr_t e;
    if((e=msi_reserve_irq(m)) < 0) return e;
    uint8_t msi = e;
    if((e=irq_register(dev->irq = (MSI_BASE + msi), msi_dispatch, 0)) < 0) return e;
    m->pci_devices[msi] = dev;
    uint8_t vec = e;
    pci_device_msi_set(dev, MSI_ADDRESS, vec);
    // NOTE: If its a problem in the future. Move this out
    pci_device_msi_enable(dev);
    return 0;
}
MSIManager msi_manager={0};
