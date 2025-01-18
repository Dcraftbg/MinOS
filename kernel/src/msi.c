#include "msi.h"
#include <minos/status.h>
intptr_t msi_reserve_irq(MSIManager* m) {
    for(size_t i = 0; i < sizeof(m->irqs); ++i) {
        if(m->irqs[i] == 0xFF) continue;
        size_t j = 0;
        while(m->irqs[i] & (1 << j)) j++;
        m->irqs[i] |= 1<<j;
        return MSI_BASE + (i * 8 + j);
    }
    return -LIMITS;
}
MSIManager msi_manager={0};
