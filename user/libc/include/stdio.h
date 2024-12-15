#pragma once
// TODO: Errors. errno. Files and more
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
typedef intptr_t ssize_t;
typedef void FILE;
#define STDOUT_FILENO 0
#define STDIN_FILENO  0
#define STDERR_FILENO 0
#define stdout ((FILE*)STDOUT_FILENO)
#define stdin  ((FILE*)STDIN_FILENO )
#define stderr ((FILE*)STDERR_FILENO)
extern FILE* stddbg;
ssize_t printf  (const char* fmt, ...) __attribute__((format(printf,1,2)));
ssize_t vprintf (const char* fmt, va_list va);
ssize_t fprintf (FILE* f, const char* fmt, ...) __attribute__((format(printf,2,3)));
ssize_t vfprintf(FILE* f, const char* fmt, va_list va);
ssize_t snprintf(char* buf, size_t cap, const char* fmt, ...) __attribute__((format(printf,3,4)));
ssize_t vsnprintf(char* buf, size_t cap, const char* fmt, va_list va);
ssize_t sprintf(char* buf, const char* fmt, ...) __attribute__((format(printf,2, 3)));

FILE* fopen(const char* path, const char* mode);
FILE *fdopen(int fd, const char *mode);
size_t fread(void* buffer, size_t size, size_t count, FILE* f);
ssize_t ftell(FILE* f);
size_t fwrite(const void* restrict buffer, size_t size, size_t count, FILE* restrict f);
int fclose(FILE* f);
int fgetc(FILE* f);
int fputs(const char* restrict str, FILE* restrict stream);
int fputc(int c, FILE* f);
static int puts(const char* restrict str) {
    return fputs(str, stdout);
}
static int putchar(int ch) {
    return fwrite(&ch, 1, 1, stdout);
}
int remove(const char* path);
int rename(const char* old_filename, const char* new_filename);
int fflush(FILE* f);
enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END
};
int fseek (FILE* f, long offset, int origin);
int sscanf(const char *restrict buffer, const char *restrict fmt, ...);
#define EOF -1
