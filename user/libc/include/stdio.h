#pragma once
// TODO: Errors. errno. Files and more
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
typedef intptr_t ssize_t;
typedef struct FILE FILE;
#define STDOUT_FILENO 0
#define STDIN_FILENO  0
#define STDERR_FILENO 0
extern FILE* stdout; 
extern FILE* stdin ; 
extern FILE* stderr; 
#define _IOLBF 0
#define _IONBF 1
#define _IOFBF 2
extern FILE* stddbg;
ssize_t printf  (const char* fmt, ...) __attribute__((format(printf,1,2)));
ssize_t vprintf (const char* fmt, va_list va);
ssize_t fprintf (FILE* f, const char* fmt, ...) __attribute__((format(printf,2,3)));
ssize_t vfprintf(FILE* f, const char* fmt, va_list va);
ssize_t snprintf(char* buf, size_t cap, const char* fmt, ...) __attribute__((format(printf,3,4)));
ssize_t vsnprintf(char* buf, size_t cap, const char* fmt, va_list va);
ssize_t sprintf(char* buf, const char* fmt, ...) __attribute__((format(printf,2, 3)));

FILE* fopen(const char* path, const char* mode);
FILE* freopen(const char* path, const char* mode, FILE* f);
FILE *fdopen(int fd, const char *mode);

size_t fread(void* buffer, size_t size, size_t count, FILE* f);
ssize_t ftell(FILE* f);
#define ftello ftell 
size_t fwrite(const void* restrict buffer, size_t size, size_t count, FILE* restrict f);
int fclose(FILE* f);
int fgetc(FILE* f);
#define getchar() fgetc(stdin)
#define getc(f) fgetc(f)
int fputs(const char* restrict str, FILE* restrict stream);
int fputc(int c, FILE* f);
#define putc fputc
static int puts(const char* restrict str) {
    return fputs(str, stdout);
}
static int putchar(int ch) {
    return fwrite(&ch, 1, 1, stdout);
}
int remove(const char* path);
int rename(const char* old_filename, const char* new_filename);
int fflush(FILE* f);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int fseek (FILE* f, long offset, int origin);
#define fseeko fseek
int sscanf(const char *restrict buffer, const char *restrict fmt, ...);
#define EOF -1

const char* strerror(int e);
#define BUFSIZ 4096 

char* fgets(char* buf, size_t size, FILE* f);
int ferror(FILE* f);
int feof(FILE* f);
#define L_tmpnam 1024
char *tmpnam(char *s);
void _libc_init_streams(void);
void clearerr(FILE* f);
int ungetc(int chr, FILE* f);
int setvbuf(FILE* f, char* buf, int mode, size_t size);
FILE* tmpfile(void);
int fileno(FILE* f);

void rewind(FILE* f);
#define FILENAME_MAX 1024

void perror(const char *s);
// Things defined by MinOS
ssize_t _status_to_errno(intptr_t status);
