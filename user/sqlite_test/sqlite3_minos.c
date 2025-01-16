#ifndef _MINOS
#   error Why are you compiling sqlite3_minos, and not targetting MinOS
#endif
#define SQLITE_MUTEX_OMIT 1
#define SQLITE_OMIT_WAL   1
#define SQLITE_MUTEX_NOOP 1
#define SQLITE_OS_OTHER   1
#define SQLITE_DEBUG      1
#include <errno.h>
#include <stdio.h>
#include "sqlite3.c"
#define oslog_sink    stderr
#define oslog(...)   (fprintf(oslog_sink, __VA_ARGS__))
#define oslogln(...) (oslogln(__VA_ARGS__), fputs("\n", oslog_sink))

#define ioprefix "IO"

#if IOTRACE_ENABLE
#   define iotrace_prefix ioprefix ":[TRACE]: "
#   define iotrace(...)   oslog  (iotrace_prefix __VA_ARGS__)
#   define iotraceln(...) oslogln(iotrace_prefix __VA_ARGS__)
#else
#   define iotrace_prefix 
#   define iotrace(...)   
#   define iotraceln(...) 
#endif

#if IOWARN_ENABLE
#   define iowarn_prefix ioprefix ":[WARN]: "
#   define iowarn(...)   oslog  (iowarn_prefix __VA_ARGS__)
#   define iowarnln(...) oslogln(iowarn_prefix __VA_ARGS__)
#else
#   define iowarn_prefix 
#   define iowarn(...)   
#   define iowarnln(...) 
#endif

typedef struct {
    sqlite3_io_methods const *pMethod;  /* Always the first entry */
    FILE* f;
} minosFile;

int minosClose(sqlite3_file* sqlite_file) {
    iotraceln("minosClose(%p)", sqlite_file);
    minosFile* file = (minosFile*)sqlite_file;
    fclose(file->f);
    return SQLITE_OK;
}


int minosRead(sqlite3_file* sqlite_file, void* data, int iAmt, sqlite3_int64 iOfst) {
    int rc = SQLITE_OK;
    iotrace("minosRead(%p, %zu, iOfst=%zu)...", sqlite_file, (size_t)iAmt, (size_t)iOfst);
    minosFile* file = (minosFile*)sqlite_file;
    memset(data, 0, iAmt);
    if(ftell(file->f) != iOfst && fseek(file->f, iOfst, SEEK_SET) < 0) {
        rc=SQLITE_IOERR;
        goto end;
    }
    ssize_t n;
    if((n=fread(data, 1, iAmt, file->f)) != iAmt) {
        if(n >= 0) {
            iowarnln("read EOF. Ignored");
            // NOTE: I know its kind of stupid
            // but this is the only way I found it 
            rc = SQLITE_OK;
            // rc=SQLITE_IOERR;//SQLITE_IOERR_SHORT_READ;
            goto end;
        }
        rc=SQLITE_IOERR;
        goto end;
    }
end:
    if(rc == SQLITE_OK) iotraceln("OK");
    else iotraceln("ERR(%d)", rc);
    return rc;
}
#define SEGMENT_SIZE 4096
#define segment_align_up(n) ((((n) + (SEGMENT_SIZE-1)) / SEGMENT_SIZE) * SEGMENT_SIZE)
#include <minos/sysstd.h>
int minosFileSize(sqlite3_file* sqlite_file, sqlite3_int64 *pSize);
int minosWrite(sqlite3_file* sqlite_file, const void* data, int iAmt, sqlite3_int64 iOfst) {
    int rc = SQLITE_OK;
    iotrace("minosWrite(%p, %zu, iOfst=%zu)...", sqlite_file, (size_t)iAmt, (size_t)iOfst);
    sqlite3_int64 size;
    if((rc=minosFileSize(sqlite_file, &size)) != SQLITE_OK) 
        goto end;
    minosFile* file = (minosFile*)sqlite_file;
    size_t nsize = segment_align_up(iOfst + iAmt);
    if(size < nsize) {
        intptr_t e;
        if((e=truncate(fileno(file->f), nsize)) < 0) {
            iotrace("Failed to truncate: %d", (int)e);
            rc=SQLITE_IOERR;
            goto end;
        }
        iotrace("truncate(%p, %zu).", sqlite_file, nsize);
    }
    if(ftell(file->f) != iOfst && fseek(file->f, iOfst, SEEK_SET) < 0) {
        rc=SQLITE_IOERR;
        goto end;
    }
    ssize_t n;
    if((n=fwrite(data, 1, iAmt, file->f)) != iAmt) {
        rc=SQLITE_IOERR;
        goto end;
    }
end:
    if(rc == SQLITE_OK) iotraceln("OK");
    else iotraceln("ERR(%d)", rc);
    return rc;
}
int minosTruncate(sqlite3_file*, sqlite3_int64 size) {
    iotraceln("<minosTruncate stub>");
    return SQLITE_NOLFS;
}
int minosSync(sqlite3_file*, int flags) {
    // iotraceln("IGNORE: <minosSync stub>");
    return SQLITE_OK;
}
int minosFileSize(sqlite3_file* sqlite_file, sqlite3_int64 *pSize) {
    // iotrace("minosFileSize(%p)...", sqlite_file);
    int rc = SQLITE_OK;
    minosFile* file = (minosFile*)sqlite_file;
    long pos = ftell(file->f);
    if(pos < 0) {
        rc=SQLITE_IOERR;
        goto end;
    }
    if(fseek(file->f, 0, SEEK_END) < 0) {
        rc=SQLITE_IOERR;
        goto end;
    }
    long size = ftell(file->f);
    if(size < 0) {
        rc=SQLITE_IOERR;
        goto end;
    }
    if(fseek(file->f, pos, SEEK_SET) < 0) {
        rc = SQLITE_IOERR;
        goto end;
    }
    *pSize = size;
end:
    // if(rc == SQLITE_OK) iotraceln("OK (%zu bytes)", size);
    // else iotraceln("ERR(%d)", rc);
    return rc;
}

int minosLock(sqlite3_file*, int) {
    return SQLITE_OK;
}
int minosUnlock(sqlite3_file*, int) {
    return SQLITE_OK;
}
int minosCheckReservedLock(sqlite3_file*, int *pResOut) {
    return SQLITE_OK;
}

// ops in sqlite3.h:1215
int minosFileControl(sqlite3_file* sqlite_file, int op, void *pArg) {
    int rc = SQLITE_OK;
    iotrace("minosFileControl(%p, %d, %p)...",  sqlite_file, op, pArg);
    switch(op) {
    case SQLITE_FCNTL_HAS_MOVED:
        *((int*)pArg) = 0;
        break;
    case SQLITE_FCNTL_SYNC:
        break; 
    default:
        iotrace("Unsupported(%p, %d, %p)...", sqlite_file, op, pArg);
        rc=SQLITE_NOTFOUND;
        break;
    }
    if(rc == SQLITE_OK) iotraceln("OK");
    else iotraceln("ERR(%d)", rc);
    return rc;
}
int minosSectorSize(sqlite3_file*) {
    return SEGMENT_SIZE;
}
int minosDeviceCharacteristics(sqlite3_file*) {
    return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN | SQLITE_IOCAP_SUBPAGE_READ;
}
// flags in sqlite3.h:595
int minosOpen(sqlite3_vfs*, sqlite3_filename zName, sqlite3_file* sqlite_file,
               int flags, int *pOutFlags) {
    int rc = SQLITE_OK;
    iotrace("minosOpen(name=\"%s\", flags=0x%08X, file=%p)...", zName, flags, sqlite_file);
    static struct sqlite3_io_methods io_methods = {
        .iVersion = 1,
        .xClose = minosClose,
        .xRead = minosRead,
        .xWrite = minosWrite,
        .xTruncate = minosTruncate,
        .xSync = minosSync,
        .xFileSize = minosFileSize,
        .xLock = minosLock,
        .xUnlock = minosUnlock,
        .xCheckReservedLock = minosCheckReservedLock,
        .xFileControl = minosFileControl,
        .xSectorSize = minosSectorSize,
        .xDeviceCharacteristics = minosDeviceCharacteristics,
    };
    /*
    if(
        flags & SQLITE_OPEN_DELETEONCLOSE ||
        flags & SQLITE_OPEN_EXCLUSIVE ||
        flags & SQLITE_OPEN_AUTOPROXY ||
        flags & SQLITE_OPEN_URI ||
        flags & SQLITE_OPEN_MEMORY ||
        flags & SQLITE_OPEN_TRANSIENT_DB || 
        flags & SQLITE_OPEN_SUBJOURNAL || 
        flags & SQLITE_OPEN_SUPER_JOURNAL ||
        flags & SQLITE_OPEN_NOMUTEX || 
        flags & SQLITE_OPEN_FULLMUTEX ||
        flags & SQLITE_OPEN_SHAREDCACHE ||
        flags & SQLITE_OPEN_PRIVATECACHE || 
        flags & SQLITE_OPEN_WAL ||
        flags & SQLITE_OPEN_NOFOLLOW ||
        flags & SQLITE_OPEN_EXRESCODE)
    {
        fprintf(stderr, "(minosOpen) some flag is unsupported %08X\n", flags);
        rc=SQLITE_NOLFS;
        goto end;
    }
    */
    minosFile* file = (minosFile*)sqlite_file;
    const char* mode = NULL;
    if(flags & SQLITE_OPEN_READONLY) {
        if(flags & SQLITE_OPEN_READWRITE || flags & SQLITE_OPEN_CREATE) {
            fprintf(stderr, "(minosOpen) cannot do readonly with incompatible flags RW or CREATE: %08X\n", flags);
            rc=SQLITE_ERROR;
            goto end;
        }
        mode = "r";
    }
    if(flags & SQLITE_OPEN_READWRITE) {
        mode = "rw";
    }
    if(!mode) {
        fprintf(stderr, "(minosOpen) invalid flags. No mode. %08X", flags);
        rc=SQLITE_ERROR;
        goto end;
    }
    iotrace("mode=\"%s\"...", mode);
    file->f = fopen(zName, mode);
    if(!file->f) {
        iotrace("Failed to open...");
        rc=SQLITE_IOERR;
        goto end;
    }
    file->pMethod = &io_methods;
    if(pOutFlags) *pOutFlags = 0;
end:
    if(rc == SQLITE_OK) iotrace("OK");
    else iotrace("ERR(%d)", rc);
    iotraceln("");
    return rc;
}
int minosDelete(sqlite3_vfs*, const char *zName, int syncDir) {
    iotraceln("<minosDelete stub>");
    return SQLITE_OK;
}
int minosAccess(sqlite3_vfs*, const char *zName, int flags, int *pResOut) {
    // iotraceln("minosAccess(%s, %08X, %p)", zName, flags, pResOut);
    *pResOut = 0;
    return SQLITE_OK;// SQLITE_ERROR;
}
int minosFullPathname(sqlite3_vfs*, const char *zName, int nOut, char *zOut) {
    strncpy(zOut, zName, nOut);
    // iotraceln("minosFullPathname(%s, %d, %s)", zName, nOut, zOut);
    return SQLITE_OK;
}
void *minosDlOpen(sqlite3_vfs*, const char *zFilename) {
    iotraceln("<minosDlOpen stub>");
    return NULL;
}
void minosDlError(sqlite3_vfs*, int nByte, char *zErrMsg) {
    iotraceln("<minosDlError stub>");
}
typedef void (minos_dl_sym)(void);
minos_dl_sym* minosDlSym(sqlite3_vfs*,void*, const char *zSymbol) {
    iotraceln("<minosDlSym)(sqlite3_vfs*,void*, const char *zSymbol) stub>");
    return NULL;
}
void minosDlClose(sqlite3_vfs*, void*) {
    iotraceln("<minosDlClose stub>");
}

static uint32_t wang_seed;
static inline uint32_t wang_hash(uint32_t seed) {
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
} 
static inline uint32_t wang_rand() {
    return wang_seed = wang_hash(wang_seed);
}
int minosRandomness(sqlite3_vfs*, int nByte, char *zOut) {
    assert(nByte >= 0);
    for(size_t i = 0; i < nByte; ++i) {
        zOut[i] = wang_rand();
    }
    return SQLITE_OK;
}
int minosSleep(sqlite3_vfs*, int microseconds) {
    printf("<minosSleep stub>\n");
    return SQLITE_ERROR;
}
int minosCurrentTime(sqlite3_vfs*, double*) {
    printf("<minosCurrentTime stub>\n");
    return SQLITE_ERROR;
}
int minosGetLastError(sqlite3_vfs*, int, char *) {
    return errno;
}
int sqlite3_os_init(void) {
    static sqlite3_vfs vfs = {
        .iVersion = 1,
        .szOsFile = sizeof(minosFile),
        .mxPathname = 1024,
        .pNext = NULL,
        .zName = "minos",
        .pAppData = NULL,
        .xOpen = minosOpen,
        .xDelete = minosDelete,
        .xAccess = minosAccess,
        .xFullPathname = minosFullPathname,
        .xDlOpen = minosDlOpen,
        .xDlError = minosDlError,
        .xDlSym = minosDlSym,
        .xDlClose = minosDlClose,
        .xRandomness = minosRandomness,
        .xSleep = minosSleep,
        .xCurrentTime = minosCurrentTime,
        .xGetLastError = minosGetLastError,
    };
    sqlite3_vfs_register(&vfs, 0);
    return SQLITE_OK;
}
int sqlite3_os_end(void) {
    return SQLITE_OK;
}
