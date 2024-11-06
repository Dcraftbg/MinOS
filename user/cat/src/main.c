#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
const char* shift_args(int *argc, const char ***argv) {
    if((*argc) <= 0) return NULL;
    const char* arg = **argv;
    (*argc)--;
    (*argv)++;
    return arg;
}
void usage(FILE* sink, const char* exe) {
    fprintf(sink, "%s [file paths...]\n", exe);
}

intptr_t read_exact(uintptr_t file, void* bytes, size_t amount) {
    while(amount) {
        size_t rb = read(file, bytes, amount);
        if(rb < 0) return rb;
        if(rb == 0) return -PREMATURE_EOF;
        amount-=rb;
        bytes+=rb;
    }
    return 0;
}
intptr_t cat(const char* path) {
    intptr_t e;
    uintptr_t fd;
    if((e=open(path, MODE_READ, 0)) < 0) {
        fprintf(stderr, "Failed to open `%s`: %s\n", path, status_str(e));
        return e;
    }
    fd=e;
    if((e=seek(fd, 0, SEEK_EOF)) < 0) {
        fprintf(stderr, "Failed to seek: %s\n", status_str(e));
        goto err_seek;
    }
    size_t size = e;
    if((e=seek(fd, 0, SEEK_START)) < 0) {
        fprintf(stderr, "Failed to seek back: %s\n", status_str(e));
        goto err_seek;
    }
    char* buf = malloc(size+1);
    if(!buf) {
        fprintf(stderr, "Failed: Not enough memory :(\n");
        goto err;
    }
    if((e=read_exact(fd, buf, size)) < 0) {
        fprintf(stderr, "Failed to read: %s\n", status_str(e));
        goto err;
    }
    buf[size] = '\0';
    fputs(buf, stdout);
    free(buf);
    close(fd);
    return 0;
err:
    free(buf);
err_seek:
    close(fd);
    return e;
}
int main(int argc, const char** argv) {
    const char* arg;
    const char* exe = shift_args(&argc, &argv);
    assert(exe && "Expected exe. Found nothing");
    if(argc <= 0) {
        fprintf(stderr, "Missing arguments\n");
        usage(stderr, exe);
        return 1;
    }
    while((arg=shift_args(&argc, &argv))) {
        cat(arg);
    }
    printf("\n");
    return 0;
}
