#pragma once
#include <stdint.h>
#include <stddef.h>
int octtoi(const char* str, size_t len);
intptr_t ustar_unpack(const char* into, const char* ustar_data, size_t ustar_size);

