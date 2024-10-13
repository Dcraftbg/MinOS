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
ssize_t printf  (const char* fmt, ...) __attribute__((format(printf,1,2)));
ssize_t vprintf (const char* fmt, va_list va);
ssize_t fprintf (FILE* f, const char* fmt, ...) __attribute__((format(printf,2,3)));
ssize_t vfprintf(FILE* f, const char* fmt, va_list va);

size_t fread(void* buffer, size_t size, size_t count, FILE* f);
size_t fwrite(const void* restrict buffer, size_t size, size_t count, FILE* restrict f);
int fgetc(FILE* f);
int fputs(const char* restrict str, FILE* restrict stream);
