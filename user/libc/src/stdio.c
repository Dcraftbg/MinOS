#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strinternal.h>
typedef ssize_t (*PrintWriteFunc)(void* user, const char* data, size_t len);
#include <minos/sysstd.h>
#include <minos/status.h>
static ssize_t _fwrite_base_func(void* user, const char* data, size_t len);
typedef struct {
    char* head;
    char* end;
} SWriter;
#include <errno.h>
#define ARRAY_LEN(a) (sizeof((a))/sizeof((a)[0]))
static ssize_t status_map[] = {
    [NOT_ENOUGH_MEM]     = ENOMEM,
    [BAD_INODE]          = EINVAL,
    [INVALID_PARAM]      = EINVAL, 
    [FILE_CORRUPTION]    = EIO,
    [LIMITS]             = E2BIG,
    [NOT_FOUND]          = ENOENT,
    [UNSUPPORTED]        = ENOSYS,
    [ALREADY_EXISTS]     = EEXIST,
    [INODE_IS_DIRECTORY] = EISDIR,
    [INVALID_OFFSET]     = EINVAL,
    [BAD_DEVICE]         = EIO,
    [PERMISION_DENIED]   = EPERM,
    [PREMATURE_EOF]      = EIO,
    [INVALID_MAGIC]      = ENOEXEC,
    [NO_ENTRYPOINT]      = EINVAL,
    [INVALID_TYPE]       = EINVAL,
    [INVALID_HANDLE]     = EINVAL,
    [SIZE_MISMATCH]      = EMSGSIZE,
    [WOULD_SEGFAULT]     = EFAULT,
    [RESOURCE_BUSY]      = EBUSY,
    [YOU_ARE_CHILD]      = ECHILD,
    [INVALID_PATH]       = EINVAL,
    [IS_NOT_DIRECTORY]   = ENOTDIR,
    [BUFFER_TOO_SMALL]   = ENOBUFS,
};
static ssize_t status_to_errno(intptr_t status) {
    if(status >= 0) return 0;
    status = -status;
    if(status >= ARRAY_LEN(status_map)) return EUNKNOWN;
    return status_map[status];
}
FILE* stddbg = NULL;
static ssize_t _fwrite_base_func(void* user, const char* data, size_t len);
static ssize_t print_base(void* user, PrintWriteFunc func, const char* fmt, va_list list) {
#define FUNC_CALL(user, data, len) \
    do {\
        size_t __len = len;\
        ssize_t _res = func(user, data, __len);\
        if(_res < 0) return -status_to_errno(_res);\
        else if(_res != __len) {\
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
        char pad_with = ' ';
        if(fmt[0] == '0') {
            pad_with = '0';
            fmt++;
        }
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
            count = strlen(bytes);
        } break;
        default:
            fprintf(stderr, "Unknown formatter: %c\n", *fmt);
            exit(1);
            return -INVALID_PARAM; // -(*fmt);
        }
        if (pad < 0) {
            pad = (-pad) < count ? 0 : (pad+count);
        } else {
            pad = pad < count ? 0 : pad-count;
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
    return fmt-fmt_start;
}

ssize_t printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ssize_t result = vprintf(fmt, args);
    va_end(args);
    return result;
}
static ssize_t _swrite_base_func(void* user, const char* data, size_t len) {
    SWriter* swriter = (SWriter*)user;
    size_t cap = (swriter->end-swriter->head);
    if(cap < len) len=cap;
    memcpy(swriter->head, data, len);
    swriter->head+=len;
    return len;
}
ssize_t vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    if(size == 0) return 0;
    SWriter writer={
        .head=buf,
        .end=buf+size-1,
    };
    ssize_t result = print_base(&writer, _swrite_base_func, fmt, args);
    if(result < 0) return result;
    writer.head[0] = '\0';
    return result;
}
ssize_t snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ssize_t result = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return result;
}
ssize_t vsprintf(char* buf, const char* fmt, va_list args) {
    //                              v----- Artificial sprintf limit
    ssize_t result = vsnprintf(buf, 100000, fmt, args);
    return result;
}
ssize_t sprintf(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ssize_t result = vsprintf(buf, fmt, args);
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

FILE* fopen(const char* path, const char* mode) {
    fmode_t fmode = 0;
    while(mode[0]) {
        switch(mode[0]) {
        case 'r':
            fmode |= MODE_READ;
            break;
        case 'w':
            fmode |= MODE_WRITE;
            break;
        case 'b':
            break;
        default:
            // '...' Unknown formatter
            return NULL;
        }
        mode++;
    }
    
    intptr_t e = open(path, fmode, fmode & MODE_WRITE ? O_CREAT : 0);
    if(e < 0) {
        errno = status_to_errno(e);
        return NULL;
    }
    return (FILE*)e;
}

int fclose(FILE* f) {
    return f ? close((uintptr_t)f) : EOF;
}

int fflush(FILE* f) {
    (void)f;
    return 0;
}
// #define LOG_FT_FS
int fseek (FILE* f, long offset, int origin) {
#ifdef LOG_FT_FS
    fprintf(stderr, "fseek(%p, %p, %d)... ", f, (void*)offset, origin);
    intptr_t res = seek((uintptr_t)f, offset, origin);
    if(res > 0) fprintf(stderr, "%p\n", (void*)res);
    else        fprintf(stderr, "Error: %d\n", (int)res);
    return res;
#endif
    return seek((uintptr_t)f, offset, origin);
}
ssize_t ftell(FILE* f) {
#ifdef LOG_FT_FS
    fprintf(stderr, "ftell(%p)... ", f);
    intptr_t res = tell((uintptr_t)f);
    if(res > 0) fprintf(stderr, "%p\n", (void*)res);
    else        fprintf(stderr, "Error: %d\n", (int)res);
    return res;
#else
    return tell((uintptr_t)f);
#endif
}
int rename(const char* old_filename, const char* new_filename) {
    fprintf(stderr, "ERROR: Unimplemented `rename` (%s, %s)\n", old_filename, new_filename);
    return -1;
}

int remove(const char* path) {
    fprintf(stderr, "ERROR: Unimplemented `remove` (%s)\n", path);
    return -1;
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
int fputc(int c, FILE* f) {
    fwrite(&c, 1, 1, f);
    return 0;
}
int sscanf(const char *restrict buffer, const char *restrict fmt, ...) {
    fprintf(stderr, "ERROR: Unimplemented `sscanf` (%s, %s)", buffer, fmt);
    return -1;
}
FILE *fdopen(int fd, const char *mode) {
    fprintf(stderr, "ERROR: fdopen is a stub (%d, %s)\n", fd, mode);
    return NULL;
}

static const char* strerror_str_map[] = {
    [0]          = "OK",
    [E2BIG]      = "Too Big",
    [EACCES]     = "Access Denied",
    [EADDRINUSE] = "Address in Use",
    [EIO]        = "IO Error",
    [EPERM]      = "Permission Denied",
    [ENOENT]     = "Not Found"
};
const char* strerror(int e) {
    if(e < 0) return "UNKOWN";
    if(e >= ARRAY_LEN(strerror_str_map)) return "INVALID ERROR";
    return strerror_str_map[e];
}
