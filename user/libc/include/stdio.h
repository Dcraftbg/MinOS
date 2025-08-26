#pragma once
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif
typedef struct FILE FILE;
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
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* f);
int fclose(FILE* f);
int fgetc(FILE* f);
#define getchar() fgetc(stdin)
#define getc(f) fgetc(f)
int fputs(const char* str, FILE* stream);
int fputc(int c, FILE* f);
#define putc fputc
int puts(const char* str);
static int putchar(int ch) {
    return fwrite(&ch, 1, 1, stdout);
}
int remove(const char* path);
int rename(const char* old_filename, const char* new_filename);
int fflush(FILE* f);
int fseek (FILE* f, long offset, int origin);
#define fseeko fseek
int sscanf(const char *buffer, const char *fmt, ...);
int fscanf(FILE *stream, const char *fmt, ...);
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
