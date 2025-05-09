#pragma once
#include "../../devices/tty/tty.h"

void init_chrtty(void);
Tty* chrtty_new(Inode* inode);
