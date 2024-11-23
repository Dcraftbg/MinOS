#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "utils.h"
#include "assert.h"
#include "string.h"
#include <minos/status.h>
enum {
    LOG_ALL,
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_NONE,
    LOG_COUNT
};
enum {
    LOG_COLOR_RESET,
    LOG_COLOR_GRAY,
    LOG_COLOR_WHITE,
    LOG_COLOR_BLUE,
    LOG_COLOR_YELLOW,
    LOG_COLOR_RED,
    LOG_COLOR_DARKRED,
    LOG_COLOR_COUNT
};
typedef struct Logger {
    intptr_t (*log)(struct Logger* this, uint32_t kind, const char* fmt, va_list args);
    /* Optional features in case you want logger to make you a default implementation of log */
    intptr_t (*write_str)(struct Logger* this, const char* str, size_t len);
    // NOTE: NULL for UNSUPPORTED
    intptr_t (*draw_color)(struct Logger* this, uint32_t color);
    uint32_t level;
    void* private;
} Logger;

static void logger_log_va(Logger* logger, uint32_t level, const char* fmt, va_list args) {
    if(level < logger->level) return;
    debug_assert(logger->log);
    logger->log(logger, level, fmt, args);
}
static void logger_log(Logger* logger, uint32_t level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_log_va(logger, level, fmt, args);
    va_end(args);
}

extern int logger_color_map[LOG_COUNT];
extern const char* logger_str_map[LOG_COUNT];
const char* get_ansi_color_from_log(uint32_t color);
intptr_t logger_init(Logger* logger);
intptr_t logger_log_default(Logger* logger, uint32_t level, const char* fmt, va_list args);
void init_loggers();
