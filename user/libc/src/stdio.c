#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <minos/fsdefs.h>
#include <minos/sysstd.h>
#include <minos/status.h>
#include <minos2errno.h>
#include <errno.h>

FILE* stdout = NULL; 
FILE* stdin  = NULL; 
FILE* stderr = NULL; 
enum {
    FILE_MODE_READ =0b1,
    FILE_MODE_WRITE=0b10
};
struct FILE {
    bool tmp;
    int oflags; 
    union {
        int fd;
        struct {
            char* data;
            size_t len, cap;
            off_t cursor;
        } tmp;
    } as;
    bool buf_real;
    uint8_t buf_mode;
    char* buf;
    size_t buf_len;
    errno_t error;
};
typedef ssize_t (*PrintWriteFunc)(void* user, const char* data, size_t len);
static ssize_t _fwrite_base_func(void* user, const char* data, size_t len);
typedef struct {
    char* head;
    char* end;
} SWriter;
#define ARRAY_LEN(a) (sizeof((a))/sizeof((a)[0]))
FILE* stddbg = NULL;
static ssize_t _fwrite_base_func(void* user, const char* data, size_t len);


static size_t ulltostr(char* buf, unsigned long long value, const char* digits, unsigned long long base) {
    size_t at = 0;
    if(value == 0) {
        buf[at++] = digits[0];
        return at;
    }
    while(value > 0) {
        unsigned long long k = value % base;
        value /= base;
        buf[at++] = digits[k];
    }
    for(size_t i = 0; i < at/2; ++i) {
        char c = buf[i];
        buf[i] = buf[at-i-1];
        buf[at-i-1] = c;
    }
    return at;
}
static size_t lltostr(char* buf, long long value, const char* digits, unsigned long long base) {
    return value < 0 ? (buf[0] = '-', ulltostr(buf+1, (unsigned long long)(-value), digits, base) + 1) : ulltostr(buf, value, digits, base);
}
static unsigned long long load_int_prefix(const char* fmt, char** end, va_list list) {
    unsigned long long value = 0;
    switch(*fmt) {
    case 'c':
        // NOTE: char is promoted to int
        value = (unsigned long long) va_arg(list, unsigned int);
        break;
    case 'x':
    case 'X':
    case 'u':
        value = (unsigned long long) va_arg(list, unsigned int);
        break;
    case 'd':
    case 'i':
        value = (unsigned long long) va_arg(list, int);
        break;
    case 'z':
        fmt++;
        value = (unsigned long long) va_arg(list, size_t);
        break;
    case 'l':
        fmt++;
        if(fmt[0] == 'l') {
            fmt++;
            // NOTE: No need to sign extend ull
            value = (unsigned long long) va_arg(list, unsigned long long);
        } else {
            if (sizeof(unsigned long) != sizeof(unsigned long long)) { 
                // long is not unsigned long long sized.
                // We need to sign extend
                value = fmt[0] == 'd' || fmt[0] == 'i' ?
                    (unsigned long long) va_arg(list, long) :
                    (unsigned long long) va_arg(list, unsigned long);
            } else value = (unsigned long long) va_arg(list, unsigned long);
        }
        break;
    }
    *end = (char*)fmt;
    return value;
}

static const char hex_upper_digits[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static const char hex_lower_digits[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
static size_t dtostr(char* buf, double value) {
    size_t at = lltostr(buf, (long long)value, hex_lower_digits, 10);
    const size_t prec = 4;
    value = value - (double)((long long)value);
    if(value != 0) {
        buf[at++] = '.';
        for(size_t i = 0; i < prec; ++i) {
            value *= 10;
        }
        at += lltostr(buf + at, (long long)value, hex_lower_digits, 10);
    }
    return at;
}
static ssize_t print_base(void* user, PrintWriteFunc func, const char* fmt, va_list list) {
#define FUNC_CALL(user, data, len) \
    do {\
        size_t __len = len;\
        ssize_t _res = func(user, data, __len);\
        if(_res < 0) return -_minos2errno(_res);\
        n += _res;\
        if(_res != __len) {\
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
        char* end;
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
            pad = strtol(fmt, &end, 10);
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
                prec = strtol(fmt, &end, 10);
                fmt = end;
            }
            (void)prec;
        }
        unsigned long long value = load_int_prefix(fmt, &end, list);
        fmt = end;
        bytes = ibuf;
        switch(*fmt) {
        case '%':
            // fallthrough
        case '\0':
            ibuf[0] = '%';
            count = 1;
            break;
        case 'u':
            count = ulltostr(ibuf, value, hex_lower_digits, 10);
            break;
        case 'd':
        case 'i':
            count = lltostr(ibuf, (long long) value, hex_lower_digits, 10);
            break;
        case 'c':
            ibuf[0] = (char)value;
            count = 1;
            break;
        case 'x':
            count = ulltostr(ibuf, value, hex_lower_digits, 16);
            break;
        case 'X':
            count = ulltostr(ibuf, value, hex_upper_digits, 16);
            break;
        case 'p':
            count = 16;
            value = (unsigned long long)va_arg(list, void*);
            for(size_t i = 0; i < 16; ++i) {
                ibuf[15-i] = hex_upper_digits[value & 0xF];
                value >>= 4;
            }
            break;
        case 's':
            bytes = va_arg(list, const char*);
            if(!bytes) bytes = "nil";
            count = strlen(bytes);
            break;
        case 'f':
        case 'g':
            count = dtostr(ibuf, va_arg(list, double));
            break;
        default:
            fprintf(stderr, "Unknown formatter: `%c`\n", *fmt);
            exit(1);
            return -EINVAL; // -(*fmt);
        }
        if(*fmt) fmt++;
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
    return n;
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
    return fwrite(data, 1, len, (FILE*)user);
}

static int parse_mode_to_oflags(const char* mode_str) {
    uint8_t oflags = 0;
    while(mode_str[0]) {
        switch(mode_str[0]) {
        case 'r':
            oflags |= O_READ;
            break;
        case 'w':
            oflags |= O_WRITE | O_CREAT;
            break;
        case 'b':
            break;
        default:
            return 0;
        }
        mode_str++;
    }
    if((!(oflags & O_READ)) && (oflags & O_WRITE)) oflags |= O_TRUNC;
    return oflags;

}
static FILE* filefd_new(int oflags, int fd) {
    char* vbuf = malloc(BUFSIZ);
    if(!vbuf) {
        errno = ENOMEM;
        return NULL;
    }
    FILE* f = calloc(1, sizeof(FILE));
    if(!f) {
        errno = ENOMEM;
        free(vbuf);
        return NULL;
    }
    f->oflags = oflags;
    f->as.fd = fd;
    f->buf_real = true;
    f->buf = vbuf;
    return f;
}
static FILE* filetmp_new(int oflags) {
    char* vbuf = malloc(BUFSIZ);
    if(!vbuf) {
        errno = ENOMEM;
        return NULL;
    }
    FILE* f = calloc(1, sizeof(FILE));
    if(!f) {
        errno = ENOMEM;
        free(vbuf);
        return NULL;
    }
    f->oflags = oflags;
    f->tmp = true;
    f->buf_real = true;
    f->buf = vbuf;
    return f;
}
FILE* tmpfile(void) {
    return filetmp_new(O_RDWR);
}
FILE* fopen(const char* path, const char* mode_str) {
    int oflags = parse_mode_to_oflags(mode_str);
    if(oflags == 0) {
        errno = EINVAL;
        return NULL;
    }
    intptr_t e = open(path, oflags);
    if(e < 0) {
        errno = _minos2errno(e);
        return NULL;
    }
    FILE* f = filefd_new(oflags, e);
    if(!f) {
        close(e);
        return NULL;
    }
    if((oflags & O_RDWR) == O_RDWR) {
        f->buf_mode = _IONBF;
    }
    return f;
}
// TODO: Actual implementation of this
FILE* freopen(const char* path, const char* mode, FILE* f) {
    fclose(f);
    return fopen(path, mode);
}

int fclose(FILE* f) {
    if(!f) return EOF;
    int e;
    if((e=fflush(f)) < 0) return e;
    if(f->tmp) {
        free(f->as.tmp.data);
        f->as.tmp.data = NULL;
        f->as.tmp.len = 0;
    } else close(f->as.fd);
    if(f->buf_real) free(f->buf);
    free(f);
    return 0;
}
int fseek(FILE* f, long offset, int origin) {
    if(f->oflags & O_WRITE) {
        int e = fflush(f);
        if(e < 0) return e;
    } else {
        f->buf_len = 0;
    }
    if(f->tmp) {
        switch(origin) {
        case SEEK_SET:
            f->as.tmp.cursor = offset;
            break;
        case SEEK_CUR:
            // TODO: Boundary check here
            f->as.tmp.cursor += offset;
            break;
        case SEEK_END:
            f->as.tmp.cursor = f->as.tmp.len;
            break;
        default:
            return EINVAL;
        }
        return 0;
    }
    return _minos2errno(seek(f->as.fd, offset, origin));
}
ssize_t ftell(FILE* f) {
    if(f->tmp) return f->as.tmp.cursor;
    intptr_t e = tell(f->as.fd);
    if(e < 0) return -(errno=_minos2errno(e));
    return e;
}
int rename(const char* old_filename, const char* new_filename) {
    fprintf(stderr, "ERROR: Unimplemented `rename` (%s, %s)\n", old_filename, new_filename);
    return -1;
}

int remove(const char* path) {
    fprintf(stderr, "ERROR: Unimplemented `remove` (%s)\n", path);
    return -1;
}
static ssize_t fread_uncached(FILE* f, void* buf, size_t len) {
    if(f->tmp) {
        if(f->as.tmp.cursor + len > f->as.tmp.len) {
            size_t copied = f->as.tmp.len - f->as.tmp.cursor;
            memcpy(buf, f->as.tmp.data + f->as.tmp.cursor, copied);
            f->as.tmp.cursor += copied;
            f->error = EOF;
            return copied;
        }
        memcpy(buf, f->as.tmp.data + f->as.tmp.cursor, len);
        f->as.tmp.cursor += len;
        return len;
    }
    ssize_t res = read(f->as.fd, buf, len);
    if(res < 0) {
        f->error = errno = _minos2errno(res);
        return -errno;
    } 
    if(res == 0) {
        f->error = EOF;
        return 0;
    }
    return res;
}
static size_t fbuf_eat(FILE* f, void* buf, size_t buf_len) {
    size_t n = buf_len < f->buf_len ? buf_len : f->buf_len;
    memcpy(buf, f->buf, n);
    memmove(f->buf, f->buf + n, f->buf_len-n);
    f->buf_len -= n;
    return n;
}
size_t fread(void* buffer, size_t size, size_t count, FILE* f) {
    if(size == 0 || count == 0) return 0;
    size_t bytes = size*count;
    size_t read = 0;
    ssize_t e = 0;
    switch(f->buf_mode) {
    case _IONBF:
        e=fread_uncached(f, buffer, bytes);
        if(e >= 0) read += e;
        break;
    case _IOLBF:
    case _IOFBF:
        while(bytes) {
            size_t n = fbuf_eat(f, buffer, bytes);
            buffer += n;
            bytes -= n;
            read += n;
            if(bytes == 0) break;
            e = fread_uncached(f, f->buf, BUFSIZ);
            if(e < 0) break;
            if(e == 0) break;
            f->buf_len += e;
        }
        break;
    default: 
        abort();
    }
    if(e < 0) {
        f->error = e;
        return read/size;
    }
    if(e == 0) {
        f->error = EOF;
        return read/size;
    }
    return read/size;
}
static bool tmp_reserve(FILE* f, size_t extra) {
    if(f->as.tmp.len + extra > f->as.tmp.cap) {
        size_t ncap = f->as.tmp.cap * 2 + extra;
        void* ndata = realloc(f->as.tmp.data, ncap);
        if(!ndata) return false;
        f->as.tmp.cap = ncap;
        f->as.tmp.data = ndata;
    }
    return true;
}
static ssize_t fwrite_uncached(FILE* f, const void* buf, size_t len) {
    if(f->tmp) {
        ssize_t n = f->as.tmp.cursor + len - ((ssize_t)f->as.tmp.len);
        if(n > 0) {
            if(!tmp_reserve(f, n)) {
                f->error = errno = ENOMEM;
                return -errno;
            }
            f->as.tmp.len += n;
        }
        memcpy(f->as.tmp.data + f->as.tmp.cursor, buf, len);
        f->as.tmp.cursor += len;
        return len;
    }
    ssize_t res = write(f->as.fd, buf, len);
    if(res < 0) {
        f->error = errno = _minos2errno(res);
        return -errno;
    } 
    return res;
}
static ssize_t fwrite_uncached_exact(FILE* f, const void* buf, size_t len) {
    while(len) {
        ssize_t n = fwrite_uncached(f, buf, len);
        if(n < 0) return n;
        if(n == 0) return 0;
        buf+=n;
        len-=n;
    }
    return len;
}
size_t fwrite(const void* restrict buffer, size_t size, size_t count, FILE* restrict f) {
    if(!f) {
        errno = EINVAL;
        return 0;
    }
    if(size == 0 || count == 0) return 0;
    size_t bytes = size*count;
    ssize_t e;
    switch(f->buf_mode) {
    case _IONBF:
        e = fwrite_uncached(f, buffer, bytes);
        break;
    case _IOLBF:
        for(size_t i = 0; i < bytes; ++i) {
            if(f->buf_len >= BUFSIZ) {
                e = fflush(f);
                if(e < 0) break;
            }
            f->buf[f->buf_len++] = ((const char*)buffer)[i];
            if(((const char*)buffer)[i] == '\n') {
                e = fflush(f);
                if(e < 0) break;
            }
        }
        e = bytes;
        break;
    case _IOFBF:
        for(size_t i = 0; i < bytes; ++i) {
            if(f->buf_len >= BUFSIZ) {
                e = fflush(f);
                if(e < 0) break;
            }
            f->buf[f->buf_len++] = ((const char*)buffer)[i];
        }
        e = bytes;
        break;
    default:
        abort();
    }
    if(e < 0) {
        f->error = -e;
        return 0;
    }
    return e/size;
}
int fflush(FILE* f) {
    if(!(f->oflags & O_WRITE)) return 0;
    if(f->buf_len == 0) return 0;
    ssize_t n = fwrite_uncached_exact(f, f->buf, f->buf_len);
    if(n < 0) return n;
    f->buf_len = 0;
    return 0;
}
int fgetc(FILE* f) {
    char c;
    if(fread(&c, sizeof(c), 1, f) != 1) return -1;
    return c;
}

char* fgets(char* buf, size_t size, FILE* f) {
    if(size == 0) return buf;
    size_t i = 0;
    int c = 0;
    while(i < (size-1) && (c=fgetc(f)) > 0) {
        buf[i++] = c;
        if(c == '\n') break;
    }
    if(c < 0) return NULL;
    buf[i++] = '\0';
    return buf;
}

#include <string.h>
// TODO: Error check
int fputs(const char* restrict str, FILE* restrict stream) {
    if(fwrite(str, strlen(str), 1, stream) == 0) return 0;
    return strlen(str);
}
int fputc(int c, FILE* f) {
    fwrite(&c, 1, 1, f);
    return 0;
}
int sscanf(const char *restrict buffer, const char *restrict fmt, ...) {
    fprintf(stderr, "ERROR: Unimplemented `sscanf` (%s, %s)\n", buffer, fmt);
    return -1;
}
int fscanf(FILE *stream, const char *restrict fmt, ...) {
    fprintf(stderr, "ERROR: Unimplemented `fscanf` (%p, %s)\n", stream, fmt);
    return -1;
}
FILE *fdopen(int fd, const char* mode_str) {
    int oflags = parse_mode_to_oflags(mode_str);
    if(!oflags) {
        errno = EINVAL;
        return NULL;
    }
    // FIXME: ignores mode entirely.
    return filefd_new(oflags, fd);
}

// Generated with errno_extract.c
static const char* strerror_str_map[] = {
    [0]               = "Success",
    [EPERM]           = "Operation not permitted",
    [ENOENT]          = "No such file or directory",
    [ESRCH]           = "No such process",
    [EINTR]           = "Interrupted system call",
    [EIO]             = "Input/output error",
    [ENXIO]           = "No such device or address",
    [E2BIG]           = "Argument list too long",
    [ENOEXEC]         = "Exec format error",
    [EBADF]           = "Bad file descriptor",
    [ECHILD]          = "No child processes",
    [EAGAIN]          = "Resource temporarily unavailable",
    [ENOMEM]          = "Cannot allocate memory",
    [EACCES]          = "Permission denied",
    [EFAULT]          = "Bad address",
    [ENOTBLK]         = "Block device required",
    [EBUSY]           = "Device or resource busy",
    [EEXIST]          = "File exists",
    [EXDEV]           = "Invalid cross-device link",
    [ENODEV]          = "No such device",
    [ENOTDIR]         = "Not a directory",
    [EISDIR]          = "Is a directory",
    [EINVAL]          = "Invalid argument",
    [ENFILE]          = "Too many open files in system",
    [EMFILE]          = "Too many open files",
    [ENOTTY]          = "Inappropriate ioctl for device",
    [ETXTBSY]         = "Text file busy",
    [EFBIG]           = "File too large",
    [ENOSPC]          = "No space left on device",
    [ESPIPE]          = "Illegal seek",
    [EROFS]           = "Read-only file system",
    [EMLINK]          = "Too many links",
    [EPIPE]           = "Broken pipe",
    [EDOM]            = "Numerical argument out of domain",
    [ERANGE]          = "Numerical result out of range",
    [EDEADLK]         = "Resource deadlock avoided",
    [ENAMETOOLONG]    = "File name too long",
    [ENOLCK]          = "No locks available",
    [ENOSYS]          = "Function not implemented",
    [ENOTEMPTY]       = "Directory not empty",
    [ELOOP]           = "Too many levels of symbolic links",
    [ENOMSG]          = "No message of desired type",
    [EIDRM]           = "Identifier removed",
    [ENOSTR]          = "Device not a stream",
    [ENODATA]         = "No data available",
    [ETIME]           = "Timer expired",
    [ENOSR]           = "Out of streams resources",
    [ENOLINK]         = "Link has been severed",
    [EPROTO]          = "Protocol error",
    [EMULTIHOP]       = "Multihop attempted",
    [EBADMSG]         = "Bad message",
    [EOVERFLOW]       = "Value too large for defined data type",
    [EILSEQ]          = "Invalid or incomplete multibyte or wide character",
    [ENOTSOCK]        = "Socket operation on non-socket",
    [EDESTADDRREQ]    = "Destination address required",
    [EMSGSIZE]        = "Message too long",
    [EPROTOTYPE]      = "Protocol wrong type for socket",
    [ENOPROTOOPT]     = "Protocol not available",
    [EPROTONOSUPPORT] = "Protocol not supported",
    [ENOTSUP]         = "Operation not supported",
    [EAFNOSUPPORT]    = "Address family not supported by protocol",
    [EADDRINUSE]      = "Address already in use",
    [EADDRNOTAVAIL]   = "Cannot assign requested address",
    [ENETDOWN]        = "Network is down",
    [ENETUNREACH]     = "Network is unreachable",
    [ENETRESET]       = "Network dropped connection on reset",
    [ECONNABORTED]    = "Software caused connection abort",
    [ECONNRESET]      = "Connection reset by peer",
    [ENOBUFS]         = "No buffer space available",
    [EISCONN]         = "Transport endpoint is already connected",
    [ENOTCONN]        = "Transport endpoint is not connected",
    [ETIMEDOUT]       = "Connection timed out",
    [ECONNREFUSED]    = "Connection refused",
    [EHOSTUNREACH]    = "No route to host",
    [EALREADY]        = "Operation already in progress",
    [EINPROGRESS]     = "Operation now in progress",
    [ESTALE]          = "Stale file handle",
    [EDQUOT]          = "Disk quota exceeded",
    [ECANCELED]       = "Operation canceled",
    [EOWNERDEAD]      = "Owner died",
    [ENOTRECOVERABLE] = "State not recoverable",
};
const char* strerror(int e) {
    if(e < 0) return "UNKNOWN";
    if(e >= ARRAY_LEN(strerror_str_map)) return "INVALID ERROR";
    return strerror_str_map[e];
}

int ferror(FILE* f) {
    return f->error == EOF ? 0 : f->error;
}
int feof(FILE* f) {
    return f->error == EOF;
}
#include <assert.h>
void _libc_init_streams(void) {
    stdout = fdopen(STDOUT_FILENO, "wb");
    stdin  = fdopen( STDIN_FILENO, "rb");
    stderr = fdopen(STDERR_FILENO, "wb");
    stdout->buf_mode = _IOLBF;
    stderr->buf_mode = _IONBF;
    assert(stdout);
    assert(stdin);
    assert(stderr);
}

void clearerr(FILE* f) {
    f->error = 0;
}
int ungetc(int chr, FILE* f) {
    fprintf(stderr, "WARN: ungetc is a stub");
    exit(1);
}
int setvbuf(FILE* f, char* buf, int mode, size_t size)  {
    assert(mode == _IOFBF || mode == _IOLBF || mode == _IONBF);
    assert(size == BUFSIZ);
    f->buf_mode = mode;
    f->buf_real = false;
    if(buf) f->buf = buf;
    return 0;
}

char *tmpnam(char *s) {
    fprintf(stderr, "WARN: tmpnam is a stub");
    exit(1);
}

void rewind(FILE* f) {
    clearerr(f);
    fseek(f, 0, SEEK_SET);
}
int fileno(FILE* f) {
    if(f->tmp) return -1;
    return f->as.fd;
}

void perror(const char *s) {
    if(s) fprintf(stderr, "%s: ", s);
    fputs(strerror(errno), stderr);
    fprintf(stderr, "\n");
}
int puts(const char* restrict str) {
    int e;
    if((e=fputs(str, stdout)) < 0) return e;
    return fputc('\n', stdout);
}
