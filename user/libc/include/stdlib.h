#pragma once
#include <stddef.h>
#include <stdint.h>
void exit(int64_t code) __attribute__ ((noreturn));
void* malloc(size_t size);
void* calloc(size_t elm, size_t size);
void* realloc(void* ptr, size_t newsize);
void free(void* addr);
void _libc_internal_init_heap();
long int strtol (const char* str, char** endptr, int base);

int atoi(const char *str);
float atof(const char *str);
int abs(int num);
void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));
