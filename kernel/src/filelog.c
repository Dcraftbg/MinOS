#include "filelog.h"
#include "vfs.h"
#include "print.h"
intptr_t file_write_str(Logger* logger, const char* str, size_t len) {
    intptr_t e;
    VfsFile file;
    if((e = vfs_open_abs((const char*)logger->private, &file, MODE_WRITE | MODE_STREAM)) < 0) {
        if(e != -NOT_FOUND) 
            goto open_err;
        if((e=vfs_create_abs((const char*)logger->private)) < 0)
            goto open_err;
        if((e = vfs_open_abs((const char*)logger->private, &file, MODE_WRITE | MODE_STREAM)) < 0)
            goto open_err;
    }
    if((e = vfs_seek(&file, 0, SEEK_EOF)) < 0)
        goto seek_err;
    if((e = vfs_write(&file, str, len)) < 0)
        goto write_err;
    vfs_close(&file);
    return e;
write_err:
seek_err:
    vfs_close(&file);
open_err:
    return e;
}
Logger file_logger = {
    .log = NULL,
    .write_str=file_write_str,
    .draw_color = NULL,
    .level = LOG_ALL,
    .private = "/kernel.log"
};
