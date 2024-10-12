#pragma once
#include <stdarg.h> // for va_arg(), va_list()
#include "serial.h"
#include "utils.h"

#define STB_SPRINTF_NOFLOAT 
#include <stb_sprintf.h>

int printf(const char* fmt, ...) PRINTFLIKE(1, 2);
