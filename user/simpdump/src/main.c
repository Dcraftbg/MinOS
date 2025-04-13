#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define eprintfln(...) (eprintf(__VA_ARGS__), fputs("\n", stderr))

#define defer_return(x) do { result = (x); goto DEFER; } while(0)
const char* read_entire_file(const char* path, size_t* size) {
    char* result = NULL;
    char* head = NULL;
    char* end = NULL;
    size_t buf_size = 0;
    long at = 0;
    FILE *f = fopen(path, "rb");

    if(!f) {
        eprintfln("ERROR Could not open file %s: %s",path,strerror(errno));
        return NULL;
    }
    if(fseek(f, 0, SEEK_END) != 0) {
        eprintfln("ERROR Could not fseek on file %s: %s",path,strerror(errno));
        defer_return(NULL);
    }
    at = ftell(f);
    if(at == -1L) {
        eprintfln("ERROR Could not ftell on file %s: %s",path,strerror(errno));
        defer_return(NULL);
    }
    *size = at;
    buf_size = at+1;
    rewind(f); // Fancy :D
    result = malloc(buf_size);
    assert(result && "Ran out of memory");
    head = result;
    end = result+buf_size-1;
    while(head != end) {
        head += fread(head, 1, end-head, f);
        if(ferror(f)) {
            eprintfln("ERROR Could not fread on file %s: %s",path,strerror(errno));
            free(result);
            defer_return(NULL);
        }
        if(feof(f)) {
            *size = buf_size = head-result;
            eprintfln("PREMATURE EOF. Was able to read: %zu", buf_size);
            break;
        }
        // TODO: Think about checking with feof
    }
    result[buf_size-1] = '\0';
DEFER:
    fclose(f);
    return result;
}
static void dump_hex(const uint8_t* bytes, size_t size) {
    for(size_t i = 0; i < size; ++i) { 
        if(i > 0) {
            printf(" ");
            if(i % 8 == 0) printf(" ");
            if(i % 16 == 0) printf("\n");
        }
        if(i % 16 == 0) printf("%016lX> ", i);
        const char* hex_digits = "0123456789abcdef";
        putchar(hex_digits[(bytes[i] >> 4) & 0xF]);
        putchar(hex_digits[(bytes[i] >> 0) & 0xF]);
    }
    printf("\n");
}
const char* shift_args(int *argc, const char ***argv) {
    if((*argc) <= 0) return NULL;
    return ((*argc)--, *((*argv)++));
}
int main(int argc, const char** argv) {
    const char* exe = shift_args(&argc, &argv);
    assert(exe);
    const char* ipath = NULL;
    size_t n = 0; 
    size_t s = 0;
    const char* arg;
    while((arg=shift_args(&argc, &argv))) {
        if(strcmp(arg, "-s") == 0) {
            const char* nstr = shift_args(&argc, &argv);
            if(!nstr) {
                fprintf(stderr, "Expected number after -s\n");
                return 1;
            }
            // TODO: check if its a number or not
            s = atoi(nstr);
        }
        else if(strcmp(arg, "-n") == 0) {
            const char* nstr = shift_args(&argc, &argv);
            if(!nstr) {
                fprintf(stderr, "Expected number after -n\n");
                return 1;
            }
            // TODO: check if its a number or not
            n = atoi(nstr);
        }
        else if(!ipath) ipath = arg;
        else {
            fprintf(stderr, "ERROR: Unexpected argument `%s`\n", arg);
            return 1;
        }
    }
    if(!ipath) {
        fprintf(stderr, "ERROR: Missing input path!\n");
        return 1;
    }
    size_t size = 0;
    const char* buf = read_entire_file(ipath, &size);
    if(!buf) return 1;
    if(s + n > size) {
        fprintf(stderr, "ERROR: Invalid offset!\n");
        return 1;
    }
    if(!n) n = size-s;
    dump_hex((const uint8_t*)buf + s, n);
    return 0;
}
