#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "print.h"
#include "print_base.h"

static void print_sink(void* _, const char* data, size_t len) {
    serial_print(data, len);
}
int printf(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int c = print_base(NULL, print_sink, fmt, va);
    va_end(va);
    return c;
}
