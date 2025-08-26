#include "printf.h"
#include "string.h"
#include "strinternal.h"
#include <unistd.h>
typedef ssize_t (*PrintWriteFunc)(void* user, const char* data, size_t len);

#define EINVAL 1
static ssize_t print_base(void* user, PrintWriteFunc func, const char* fmt, va_list list) {
#define FUNC_CALL(user, data, len) \
    do {\
        size_t __len = len;\
        ssize_t _res = func(user, data, __len);\
        if(_res < 0) return -1;\
        n += _res;\
        if((size_t)_res != __len) {\
            return n;\
        }\
    } while(0)
    size_t n = 0;
    while(*fmt) {
        const char* begin = fmt;
        while(*fmt && *fmt != '%') fmt++;
        if(begin != fmt) {
            FUNC_CALL(user, begin, fmt-begin);
        }
        if(*fmt == '\0') break;
        
        const char* bytes = NULL;
        size_t count=0;
        const char* end;
        fmt++;
        char pad_with = ' ';
        if(fmt[0] == '0') {
            pad_with = '0';
            fmt++;
        }
        int pad;
        if(fmt[0] == '*') {
            pad = va_arg(list, int); 
            fmt++;
        } else {
            pad = atoi_internal(fmt, &end);
            fmt=end;
        }
        char ibuf[30];

        // TODO: use precision
        if(*fmt == '.') {
            int prec = 0;
            fmt++;
            if(*fmt == '*') {
                prec = va_arg(list, int);
                fmt++;
            } else {
                prec = atoi_internal(fmt, &end);
                fmt = end;
            }
            (void)prec;
        }
        switch(*fmt) {
        case '%':
            fmt++;
            // fallthrough
        case '\0': {
            static const char c='%';
            bytes = &c;
            count = 1;
        } break;
        case 'u': // <- Not really correct but I can't be bothered rn xD
        case 'i':
        case 'd': {
            fmt++;
            bytes = ibuf;
            count = itoa_internal(ibuf, sizeof(ibuf), va_arg(list, int));
        } break;
        case 'l': {
            fmt++;
            switch(*fmt) {
            case 'd':
                fmt++;
                bytes = ibuf;
                count = lltoa_internal(ibuf, sizeof(ibuf), va_arg(list, long int));
                break;
            // TODO: llutoa_internal
            case 'u':
                fmt++;
                bytes = ibuf;
                count = sztoa_internal(ibuf, sizeof(ibuf), va_arg(list, size_t));
                break;
            case 'l':
                fmt++;
                switch(*fmt) {
                case 'd':
                    fmt++;
                    bytes = ibuf;
                    count = lltoa_internal(ibuf, sizeof(ibuf), va_arg(list, long long int));
                    break;
                // TODO: llutoa_internal
                case 'u':
                    fmt++;
                    bytes = ibuf;
                    count = sztoa_internal(ibuf, sizeof(ibuf), va_arg(list, size_t));
                    break;
                default:
                    return -EINVAL;
                }
                break;
            default:
                return -EINVAL;
            }
        } break;
        case 'z': {
            fmt++;
            switch(*fmt) {
            case '\0': 
                return -EINVAL; // -'z';
            case 'u': {
                fmt++;
                bytes = ibuf;
                count = sztoa_internal(ibuf, sizeof(ibuf), va_arg(list, size_t));
            } break;
            default:
                return -EINVAL; // -(*fmt);
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
            count = utoha_internal(ibuf, sizeof(ibuf), va_arg(list, unsigned int), hex_upper_digits);
        } break;
        case 'x': {
            fmt++;
            bytes = ibuf;
            count = utoha_internal(ibuf, sizeof(ibuf), va_arg(list, unsigned int), hex_lower_digits);
        } break;
        case 'p': {
            fmt++;
            bytes = ibuf;
            count = uptrtoha_full_internal(ibuf, sizeof(ibuf), (uintptr_t)va_arg(list, void*), hex_upper_digits);
        } break;
        case 's': {
            fmt++;
            bytes = va_arg(list, const char*);
            if(!bytes) bytes = "nil";
            count = strlen(bytes);
        } break;
        }
        if (pad < 0) {
            pad = (size_t)(-pad) < count ? 0 : (pad+count);
        } else {
            pad = (size_t)pad < count ? 0 : pad-count;
        }
        while(pad > 0) {
            FUNC_CALL(user, &pad_with, 1);
            pad--;
        }
        FUNC_CALL(user, bytes, count);
        while(pad < 0) {
            FUNC_CALL(user, &pad_with, 1);
            pad++;
        }
    }
    return n;
}

static ssize_t _write_base_func(void* user, const char* data, size_t len) {
    return write((uintptr_t)user, data, len);
}

ssize_t vdprintf(int fd, const char* fmt, va_list args) {
    return print_base((void*)((uintptr_t)fd), _write_base_func, fmt, args);
}
ssize_t dprintf(int fd, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ssize_t result = vdprintf(fd, fmt, args);
    va_end(args);
    return result;
}
