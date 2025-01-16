#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strinternal.h>
#include <stdbool.h>
#include <minos/fsdefs.h>
#include <minos/sysstd.h>
#include <minos/status.h>
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
    uint8_t mode;
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
    return fwrite(data, 1, len, (FILE*)user);
}

static uint8_t parse_mode(const char* mode_str) {
    uint8_t mode = 0;
    while(mode_str[0]) {
        switch(mode_str[0]) {
        case 'r':
            mode |= FILE_MODE_READ;
            break;
        case 'w':
            mode |= FILE_MODE_WRITE;
            break;
        case 'b':
            break;
        default:
            return 0;
        }
        mode_str++;
    }
    return mode;

}
fmode_t mode_to_minos(uint8_t mode) {
    fmode_t fmode = 0;
    fmode |= (mode & FILE_MODE_READ ) ? MODE_READ  : 0;
    fmode |= (mode & FILE_MODE_WRITE) ? MODE_WRITE : 0;
    return fmode;
}
static FILE* filefd_new(uint8_t mode, int fd) {
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
    f->mode = mode;
    f->as.fd = fd;
    f->buf_real = true;
    f->buf = vbuf;
    return f;
}
static FILE* filetmp_new(uint8_t mode) {
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
    f->mode = mode;
    f->tmp = true;
    f->buf_real = true;
    f->buf = vbuf;
    return f;
}
FILE* tmpfile(void) {
    return filetmp_new(FILE_MODE_READ | FILE_MODE_WRITE);
}
FILE* fopen(const char* path, const char* mode_str) {
    uint8_t mode = parse_mode(mode_str);
    if(mode == 0) {
        errno = EINVAL;
        return NULL;
    }
    fmode_t fmode = mode_to_minos(mode);
    oflags_t oflags = fmode & MODE_WRITE ? O_CREAT : 0;
    if(mode == MODE_WRITE) oflags |= O_TRUNC;
    intptr_t e = open(path, fmode, oflags);
    if(e < 0) {
        errno = status_to_errno(e);
        return NULL;
    }
    FILE* f = filefd_new(mode, e);
    if(!f) {
        close(e);
        return NULL;
    }
    if((mode & (MODE_WRITE | MODE_READ)) == (MODE_WRITE | MODE_READ)) {
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
    if(f->mode & FILE_MODE_WRITE) {
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
    return status_to_errno(seek(f->as.fd, offset, origin));
}
ssize_t ftell(FILE* f) {
    if(f->tmp) return f->as.tmp.cursor;
    intptr_t e = tell(f->as.fd);
    if(e < 0) return -(errno=status_to_errno(e));
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
        f->error = errno = status_to_errno(res);
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
    ssize_t e;
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
        f->error = errno = status_to_errno(res);
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
    size_t bytes = size*count;
    ssize_t e;
    switch(f->buf_mode) {
    case _IONBF:
        e = fwrite_uncached(f, buffer, bytes);
        break;
    case _IOLBF:
        for(size_t i = 0; i < bytes; ++i) {
            if(f->buf_len >= BUFSIZ || ((const char*)buffer)[i] == '\n') {
                e = fflush(f);
                if(e < 0) break;
            }
            f->buf[f->buf_len++] = ((const char*)buffer)[i];
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
    if(!(f->mode & FILE_MODE_WRITE)) return 0;
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
    int c;
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
    return -errno;
}
int fputc(int c, FILE* f) {
    fwrite(&c, 1, 1, f);
    return 0;
}
int sscanf(const char *restrict buffer, const char *restrict fmt, ...) {
    fprintf(stderr, "ERROR: Unimplemented `sscanf` (%s, %s)", buffer, fmt);
    return -1;
}
FILE *fdopen(int fd, const char* mode_str) {
    uint8_t mode = parse_mode(mode_str);
    if(!mode) {
        errno = EINVAL;
        return NULL;
    }
    // FIXME: ignores mode entirely.
    return filefd_new(mode, fd);
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
    stdout->buf_mode = _IONBF;
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
    f->mode = mode;
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
