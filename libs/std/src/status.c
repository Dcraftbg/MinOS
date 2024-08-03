#include <minos/status.h>
 // TODO: Move utils to minos/utils.h
#include <utils.h>
static const char* status_map[] = {
    "OK",
    "Not enough memory",
    "Bad Inode",
    "Invalid Param",
    "File Corruption",
    "Limits",
    "Not found",
    "Unsupported",
    "Already Exists",
    "Inode Is Directory",
    "Invalid Offset",
    "Bad Device",
    "Permission Denied",
    "Reached EOF prematurely",
    "Invalid Magic",
    "No Entry Point",
    "Invalid Type",
    "Invalid Handle",
    "Size Mismatch",
    "Would Segfault",
};
static_assert(STATUS_COUNT==ARRAY_LEN(status_map), "You need to update the status map!");
const char* status_str(intptr_t status) {
    if(status >= 0) return "OK";
    status = -status;
    if(status >= STATUS_COUNT) {
       return NULL;
    }
    return status_map[status];
}
