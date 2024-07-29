#pragma once
#include <stdarg.h> // for va_arg(), va_list()
#include "serial.h"
#include "utils.h"

#define STB_SPRINTF_NOFLOAT 
#include <stb_sprintf.h>

#define KERNEL_PRINTF_TMP 1024
void printf(const char* fmt, ...) PRINTFLIKE(1, 2);
