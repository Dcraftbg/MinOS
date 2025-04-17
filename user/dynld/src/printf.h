#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef intptr_t ssize_t;
#define STDERR_FILENO 0
ssize_t vdprintf(int fd, const char* fmt, va_list args);
ssize_t dprintf(int fd, const char* fmt, ...);
