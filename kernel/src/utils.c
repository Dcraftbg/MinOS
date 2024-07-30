#include "utils.h"
#include <stddef.h>
#include "assert.h"
// TODO: Better error reporting
void kabort() {
    for(;;) {
        asm volatile ("hlt");
    }
}
