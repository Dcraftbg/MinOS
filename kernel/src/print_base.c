#include <stdarg.h>
#include "string.h"
#include "print_base.h"
int print_base(void* user, PrintWriteFunc func, const char* fmt, va_list list) {
    while(*fmt) {
        const char* begin = fmt;
        while(*fmt && *fmt != '%') fmt++;
        if(begin != fmt) {
            func(user, begin, fmt-begin);
        }
        if(*fmt == '\0') break;
        
        const char* bytes = NULL;
        size_t count=0;
        const char* end;
        fmt++;
        int pad = atoi(fmt, &end);
        fmt=end;
        char ibuf[30];

        switch(*fmt) {
        case '%':
            fmt++;
        case '\0': {
            static const char c='%';
            bytes = &c;
            count = 1;
        } break;
        case 'i':
        case 'd': {
            fmt++;
            bytes = ibuf;
            count = itoa(ibuf, sizeof(ibuf), va_arg(list, int));
        } break;
        case 'z': {
            fmt++;
            switch(*fmt) {
            case '\0': 
                return 'z';
            case 'u': {
                fmt++;
                bytes = ibuf;
                count = sztoa(ibuf, sizeof(ibuf), va_arg(list, size_t));
            } break;
            default:
                return *fmt;
            }
        } break;
        case 'c': {
            fmt++;
            ibuf[0] = va_arg(list, int);
            bytes = ibuf;
            count = 1;
        } break;
        case 'X': {
            fmt++;
            bytes = ibuf;
            count = utoha(ibuf, sizeof(ibuf), va_arg(list, unsigned int));
        } break;
        case 'p': {
            fmt++;
            bytes = ibuf;
            count = uptrtoha_full(ibuf, sizeof(ibuf), (uintptr_t)va_arg(list, void*));
        } break;
        case 's': {
            fmt++;
            bytes = va_arg(list, const char*);
            count = strlen(bytes);
        } break;
        default: 
            return *fmt;
        }
        if (pad < 0) {
            pad = (-pad) < count ? 0 : (pad+count);
        } else {
            pad = pad < count ? 0 : pad-count;
        }

        while(pad > 0) {
            static const char c=' ';
            func(user, &c, 1);
            pad--;
        }
        func(user, bytes, count);
        while(pad < 0) {
            static const char c=' ';
            func(user, &c, 1);
            pad++;
        }
    }
    return 0;
}
