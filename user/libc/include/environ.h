#pragma once
#include <stddef.h>
extern char** environ;
extern size_t __environ_size;
void _libc_init_environ(const char** envv, int envc);
