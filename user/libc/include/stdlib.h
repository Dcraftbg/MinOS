#pragma once
#include <stddef.h>
#include <stdint.h>
#include "environ.h"
void exit(int64_t code) __attribute__ ((noreturn));
void* malloc(size_t size);
void* calloc(size_t elm, size_t size);
void* realloc(void* ptr, size_t newsize);
void free(void* addr);
void _libc_internal_init_heap();
void abort() __attribute__ ((noreturn));

int atoi(const char *str);
float atof(const char *str);
int abs(int num);
long labs(long num);
long long llabs(long long num);
void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));
int system(const char* command);
void *bsearch(const void *key, const void *base, size_t nitems, size_t size, int (*compar)(const void *, const void *));
char *mktemp(char *templat);
char *realpath(const char *path, char *resolved_path);

double strtod(const char* str, char** endptr);
long strtol(const char* nptr, char** endptr, int base);
long long strtoll(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
float strtof(const char *nptr, char **endptr);
#define ATEXIT_MAX 32
int atexit(void (*function)(void));
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

// For some reason my_basic requires this?
#include "rand.h"
