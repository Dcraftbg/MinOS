#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "print.h"

static char tmp_printf[KERNEL_PRINTF_TMP];
void printf(const char* fmt, ...) {
   va_list va;
   va_start(va, fmt);
   stbsp_vsnprintf(tmp_printf, KERNEL_PRINTF_TMP, fmt, va);
   va_end(va);
   serial_printstr(tmp_printf);
}
