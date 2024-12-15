#pragma once
// NOTE: Provided elsewhere
#include <stddef.h>
void *memset (void *dest, int x          , size_t n);
void *memcpy (void *dest, const void *src, size_t n);
void *memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t max);
int strncmp(const char* s1, const char* s2, size_t max);
int strcmp(const char *restrict s1, const char *restrict s2);
char* strchr(const char* str, int ch);
size_t strlen(const char* cstr);
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);
char* strdup (const char* str);
char* strrchr(const char* str, int ch);
char* strpbrk(const char* str, const char* breakset);
char* strstr (const char* str, const char* substr);
char* strncpy(char *restrict dest, const char *restrict src, size_t count);
char* strcat (char *restrict dest, const char *restrict src);
static char* strcpy (char *restrict dest, const char *restrict src) {
    return memcpy(dest, src, strlen(src)+1);
}

