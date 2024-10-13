#include <stdio.h>
#include <string.h>
#include <strinternal.h>
typedef ssize_t (*PrintWriteFunc)(void* user, const char* data, size_t len);
#include <minos/sysstd.h>
#include <minos/status.h>
static ssize_t _fwrite_base_func(void* user, const char* data, size_t len);
static ssize_t print_base(void* user, PrintWriteFunc func, const char* fmt, va_list list) {
#define FUNC_CALL(user, data, len) \
    do {\
        size_t __len = len;\
        ssize_t _res = func(user, data, __len);\
        if(_res < 0) return _res;\
        else if(_res != __len) {\
            char some_buf[30];\
            size_t _size = sztoa_internal(some_buf, sizeof(some_buf), _res);\
            write(STDOUT_FILENO, some_buf, _size);\
            return (fmt+_res)-fmt_start;\
        }\
    } while(0)
    const char* fmt_start=fmt;
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
        int pad = atoi_internal(fmt, &end);
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
            count = itoa_internal(ibuf, sizeof(ibuf), va_arg(list, int));
        } break;
        case 'z': {
            fmt++;
            switch(*fmt) {
            case '\0': 
                return -INVALID_PARAM; // -'z';
            case 'u': {
                fmt++;
                bytes = ibuf;
                count = sztoa_internal(ibuf, sizeof(ibuf), va_arg(list, size_t));
            } break;
            default:
                return -INVALID_PARAM; // -(*fmt);
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
            count = utoha_internal(ibuf, sizeof(ibuf), va_arg(list, unsigned int));
        } break;
        case 'p': {
            fmt++;
            bytes = ibuf;
            count = uptrtoha_full_internal(ibuf, sizeof(ibuf), (uintptr_t)va_arg(list, void*));
        } break;
        case 's': {
            fmt++;
            bytes = va_arg(list, const char*);
            count = strlen(bytes);
        } break;
        default: 
            return -INVALID_PARAM; // -(*fmt);
        }
        if (pad < 0) {
            pad = (-pad) < count ? 0 : (pad+count);
        } else {
            pad = pad < count ? 0 : pad-count;
        }

        while(pad > 0) {
            static const char c=' ';
            FUNC_CALL(user, &c, 1);
            pad--;
        }
        FUNC_CALL(user, bytes, count);
        while(pad < 0) {
            static const char c=' ';
            FUNC_CALL(user, &c, 1);
            pad++;
        }
    }
    return fmt-fmt_start;
}

ssize_t printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ssize_t result = vprintf(fmt, args);
    va_end(args);
    return result;
} 
ssize_t vprintf(const char* fmt, va_list args) {
    return vfprintf(stdout, fmt, args);
}
ssize_t fprintf(FILE* f, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ssize_t result = vfprintf(f, fmt, args);
    va_end(args);
    return result;
}
ssize_t vfprintf(FILE* f, const char* fmt, va_list va) {
    return print_base(f, _fwrite_base_func, fmt, va);
}

static ssize_t _fwrite_base_func(void* user, const char* data, size_t len) {
    return write((uintptr_t)user, data, len);
}

// TODO: Set error
size_t fread(void* buffer, size_t size, size_t count, FILE* f) {
    if(size == 0 || count == 0) return 0;
    ssize_t e = read((uintptr_t)f, buffer, size*count);
    if(e < 0) return 0;
    return e/size;
}

// TODO: Set error
size_t fwrite(const void* restrict buffer, size_t size, size_t count, FILE* restrict f) {
    if(size == 0 || count == 0) return 0;
    ssize_t e = write((uintptr_t)f, buffer, size*count);
    if(e < 0) return 0;
    return e/size;
}

// TODO: Error check
int fgetc(FILE* f) {
    char c;
    fread(&c, sizeof(c), 1, f);
    return c;
}

#include <string.h>
// TODO: Error check
int fputs(const char* restrict str, FILE* restrict stream) {
    fwrite(str, strlen(str), 1, stream);
    return 0;
}

