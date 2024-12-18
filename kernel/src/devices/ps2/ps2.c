#include "ps2.h"
void init_ps2() {
    static_assert(PS2_MAX_ENTRIES > 0 && is_power_of_two(PS2_MAX_ENTRIES), "PS2_MAX_ENTRIES must be a power of two!");
    ps2queue = ps2queue_construct(kernel_malloc(PS2_MAX_ENTRIES), PS2_MAX_ENTRIES-1);
    if(!ps2queue.addr) {
        printf("WARN: Failed to allocate ps2queue for the keyboard!\n");
    }
    idt_register(0x21, idt_ps2_keyboard_handler, IDT_INTERRUPT_TYPE);
    intptr_t e = 0;
    if((e = vfs_register_device("ps2keyboard", &ps2keyboard_device)) < 0) {
        printf("ERROR: Could not register ps2keyboard device: %s\n", status_str(e));
    }
}
