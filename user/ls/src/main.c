#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

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
static const char* inode_kind_map[INODE_COUNT] = {
    "DIR",
    "FILE",
    "DEVICE",
};
const char* inode_kind_str(inodekind_t kind) {
    if(kind < 0 || kind > (sizeof(inode_kind_map)/sizeof(inode_kind_map[0]))) return "Unknown";
    return inode_kind_map[kind];
}
intptr_t ls(const char* path) {
    if(path[0] == '\0') return -INVALID_PATH;
    intptr_t e=0;
    const size_t bufsize = 4096;
    char* buf = malloc(bufsize);
    Stats stats;
    if(!buf) {
        e=-NOT_ENOUGH_MEM;
        goto buf_err;
    }
    char* pathbuf = malloc(PATH_MAX);
    if(!path) {
        e=-NOT_ENOUGH_MEM;
        goto pathbuf_err;
    }
    if((e=open(path, MODE_READ, O_DIRECTORY)) < 0) {
        fprintf(stderr, "ERROR: Could not open directory `%s`: %s\n", path, status_str(e));
        goto diropen_err;
    }
    uintptr_t dirfd = e;
    printf("ls: %s\n", path);
    while((e=get_dir_entries(dirfd, (DirEntry*)buf, bufsize)) > 0) {
        size_t bytes_read = e;
        DirEntry* entry = (DirEntry*)buf;
        while(((char*)entry) < buf+bytes_read) {
            snprintf(pathbuf, 
                     PATH_MAX,
                     path[strlen(path)-1] == '/' ? 
                         "%s%s" :
                         "%s/%s"
                     , path, entry->name);
            if((e=stat(pathbuf, &stats)) < 0) {
                fprintf(stderr, "ls: Could not get stats for `%s`: %s\n", entry->name, status_str(e));
                goto skip;
            }
            printf("%6s %15s %zu bytes\n", inode_kind_str(stats.kind), entry->name, stats.size * (1<<stats.lba));
        skip:
            entry = (DirEntry*)(((char*)entry) + entry->size);
        }
    }
    if(e < 0) {
        fprintf(stderr, "ERROR: get_dir_entries failed: %s\n", status_str(e));
        goto get_dir_entries_err;
    }
    close(dirfd);
    return 0;
get_dir_entries_err:
    close(dirfd);
diropen_err:
    free(pathbuf);
pathbuf_err:
    free(buf);
buf_err:
    return e;
    /*
    intptr_t e;
    char namebuf[128];
    if((e=diropen(path, MODE_READ)) < 0) {
        fprintf(stderr, "ERROR: Could not open directory: %s\n", path);
        return e;
    }
    size_t dirid = e;
    if((e=diriter_open(dirid)) < 0) {
        fprintf(stderr, "ERROR: Could not open directory iterator: %s\n", path);
        close(dirid);
        return e;
    }
    close(dirid);
    size_t iterid = e;
    Stats stats={0};
    printf("ls: %s\n", path);
    while((e=diriter_next(iterid)) >= 0) {
        size_t entry = e;
        if((e=identify(entry, namebuf, sizeof(namebuf))) < 0) {
            fprintf(stderr, "ls: Could not identify entry: %s\n", status_str(e));
            close(entry);
            goto err;
        }
        if((e=stat(entry, &stats)) < 0) {
            fprintf(stderr, "ls: Could not get stats for `%s`: %s\n", namebuf, status_str(e));
            close(entry);
            continue;
        }
        printf("%6s %15s %zu bytes\n", inode_kind_str(stats.kind), namebuf, stats.size * (1<<stats.lba));
        close(entry);
    }

    if(e != -NOT_FOUND) {
        fprintf(stderr, "ls: Failed to iterate: %s\n", status_str(e));
        close(iterid);
        return e;
    }
    return 0;
err:
    close(iterid);
    return e;
    */
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
        ls(arg);
    }
    return 0;
}
