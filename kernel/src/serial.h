#pragma once
#include <stdint.h>
#include <stddef.h>
void serial_init();
void serial_print_chr(uint32_t c);
void serial_printstr(const char* str);
void serial_print(const char* str, size_t len);
void serial_print_sink(void* _, const char* data, size_t len);
extern struct Logger serial_logger;
