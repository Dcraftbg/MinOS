#pragma once
#include <stdio.h>
#define NEWLINE "\n"

extern const char* log_where;
void sink_log(FILE* sink, const char* mode, const char* where, const char* fmt, ...);
#define info(...)  sink_log(stdout, "INFO" , log_where, __VA_ARGS__)
#define warn(...)  sink_log(stderr, "WARN" , log_where, __VA_ARGS__)
#define error(...) sink_log(stderr, "ERROR", log_where, __VA_ARGS__)
