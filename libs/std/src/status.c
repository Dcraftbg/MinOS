#include <minos/status.h>
static const char* status_map[STATUS_COUNT] = {
    "OK",
    [NOT_ENOUGH_MEM]     = "Not enough memory",
    [BAD_INODE]          = "Bad Inode",
    [INVALID_PARAM]      = "Invalid Param",
    [FILE_CORRUPTION]    = "File Corruption",
    [LIMITS]             = "Limits",
    [NOT_FOUND]          = "Not found",
    [UNSUPPORTED]        = "Unsupported",
    [ALREADY_EXISTS]     = "Already Exists",
    [INODE_IS_DIRECTORY] = "Inode Is Directory",
    [INVALID_OFFSET]     = "Invalid Offset",
    [BAD_DEVICE]         = "Bad Device",
    [PERMISION_DENIED]   = "Permission Denied",
    [PREMATURE_EOF]      = "Reached EOF prematurely",
    [INVALID_MAGIC]      = "Invalid Magic",
    [NO_ENTRYPOINT]      = "No Entry Point",
    [INVALID_TYPE]       = "Invalid Type",
    [INVALID_HANDLE]     = "Invalid Handle",
    [SIZE_MISMATCH]      = "Size Mismatch",
    [WOULD_SEGFAULT]     = "Would Segfault",
    [RESOURCE_BUSY]      = "Resource Busy",
    [YOU_ARE_CHILD]      = "You are child",
    [INVALID_PATH]       = "Invalid Path",
    [IS_NOT_DIRECTORY]   = "Is not Directory",
    [BUFFER_TOO_SMALL]   = "Buffer too small",
    [TIMEOUT_REACHED]    = "Timeout Reached",
    [ADDR_SOCKET_FAMILY_MISMATCH] = "Mismatch between Address and socket family",
    [UNSUPPORTED_DOMAIN] = "Unsupported Domain",
    [UNSUPPORTED_SOCKET_TYPE] = "Unsupported Socket type",
    [WOULD_BLOCK] = "Would Block",
};
// static_assert(STATUS_COUNT==ARRAY_LEN(status_map), "You need to update the status map!");
const char* status_str(intptr_t status) {
    if(status >= 0) return "OK";
    status = -status;
    if(status >= STATUS_COUNT) {
       return "Undefined Error";
    }
    return status_map[status];
}
