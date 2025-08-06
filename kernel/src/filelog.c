#include "filelog.h"
#include "vfs.h"
#include "print.h"
#include "fileutils.h"
intptr_t file_write_str(Logger* logger, const char* str, size_t len) {
    intptr_t e;
    Inode* file;
    const char* path=(const char*)logger->private;
    if((e = vfs_find_abs(path, &file)) < 0) {
        if(e != -NOT_FOUND) 
            goto open_err;
        if((e=vfs_creat_abs(path, 0, &file)) < 0)
            goto open_err;
    }
    intptr_t size = inode_size(file);
    if(size < 0) goto size_err;
    e=write_exact(file, str, len, size);
    idrop(file);
    return e;
size_err:
    idrop(file);
open_err:
    return e;
}
Logger file_logger = {
    .log = logger_log_default,
    .write_str=file_write_str,
    .draw_color = NULL,
    .level = LOG_ALL,
    .private = "/kernel.log"
};
