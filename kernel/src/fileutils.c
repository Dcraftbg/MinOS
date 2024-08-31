#include "fileutils.h"

intptr_t write_exact(VfsFile* file, const void* bytes, size_t amount) {
    while(amount) {
        size_t wb = vfs_write(file, bytes, amount);
        if(wb < 0) return wb;
        amount-=wb;
        bytes+=wb;
    }
    return 0;
}

intptr_t read_exact(VfsFile* file, void* bytes, size_t amount) {
    while(amount) {
        size_t rb = vfs_read(file, bytes, amount);
        if(rb < 0) return rb;
        if(rb == 0) return -PREMATURE_EOF;
        amount-=rb;
        bytes+=rb;
    }
    return 0;
}
