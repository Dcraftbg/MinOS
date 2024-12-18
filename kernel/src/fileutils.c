#include "fileutils.h"

intptr_t write_exact(Inode* file, const void* bytes, size_t amount, off_t offset) {
    while(amount) {
        size_t wb = inode_write(file, bytes, amount, offset);
        if(wb < 0) return wb;
        amount-=wb;
        offset+=wb;
        bytes+=wb;
    }
    return 0;
}

intptr_t read_exact(Inode* file, void* bytes, size_t amount, off_t offset) {
    while(amount) {
        size_t rb = inode_read(file, bytes, amount, offset);
        if(rb < 0) return rb;
        if(rb == 0) return -PREMATURE_EOF;
        amount-=rb;
        offset+=rb;
        bytes+=rb;
    }
    return 0;
}
