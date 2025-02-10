#include "ps2.h"
#include "keyboard/keyboard.h"
#include <interrupt.h>
#include <log.h>

void init_ps2() {
    init_ps2_keyboard();
    irq_register(1, idt_ps2_keyboard_handler, 0);
    intptr_t e = 0;
    if((e = vfs_register_device("ps2keyboard", &ps2keyboard_device)) < 0) {
        kerror("Could not register ps2keyboard device: %s", status_str(e));
    }
}
