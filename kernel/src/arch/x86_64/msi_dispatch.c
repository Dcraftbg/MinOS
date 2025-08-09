#include <stdint.h>
#include <msi.h>
#include <assert.h>
#include "task_regs.h"
extern void msi_dispatch(TaskRegs* frame) { 
    debug_assert(frame->irq - MSI_BASE < MSI_COUNT);
    PciDevice* pci = msi_manager.pci_devices[frame->irq - MSI_BASE];
    debug_assert(pci);
    pci->handler(pci);
}
