#include "minos2errno.h"
#include <minos/status.h>
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
    [TIMEOUT_REACHED]    = ETIME,
    [ADDR_SOCKET_FAMILY_MISMATCH] = EINVAL,
    [UNSUPPORTED_DOMAIN] = EDOM,
    [UNSUPPORTED_SOCKET_TYPE] = EINVAL,
    [WOULD_BLOCK]        = EWOULDBLOCK,
};
ssize_t _minos2errno(intptr_t status) {
    if(status >= 0) return 0;
    status = -status;
    if(status >= ARRAY_LEN(status_map)) return EUNKNOWN;
    return status_map[status];
}
