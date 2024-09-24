#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdbool.h>
#define HALT() \
    for(;;) 

uintptr_t stdout = 0;
uintptr_t stdin = 1;

#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include "../vendor/stb_sprintf.h"
#include <minos/keycodes.h>

#define PRINTF_TMP 1024
static char tmp_printf[PRINTF_TMP]={0};

size_t strlen(const char* cstr) {
    const char* head = cstr;
    while(*head) head++;
    return head-cstr;
}
void printf(const char* fmt, ...) __attribute__((format(printf,1, 2)));
void printf(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    size_t n = stbsp_vsnprintf(tmp_printf, PRINTF_TMP, fmt, va);
    va_end(va);
    write(stdout, tmp_printf, n);
}
int main() {
    printf("Hello from /user/hello!\n");
    return 0;
}
void _start(int argc, const char** argv) {
    exit(main());
}
