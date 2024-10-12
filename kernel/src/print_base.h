#pragma once
#include <stddef.h>
typedef void (*PrintWriteFunc)(void* user, const char* data, size_t len);
int print_base(void* user, PrintWriteFunc func, const char* fmt, va_list list);
