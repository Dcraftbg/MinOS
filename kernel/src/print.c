#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "print.h"
#include "print_base.h"

int printf(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int c = print_base(NULL, serial_print_sink, fmt, va);
    va_end(va);
    return c;
}
