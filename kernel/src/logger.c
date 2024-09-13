#include "logger.h"
#include "print.h"

static const char* str_map[LOG_COUNT] = {
    "ALL",
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
    "NONE"
};
static int color_map[LOG_COUNT] = {
    LOG_COLOR_RESET,
    LOG_COLOR_GRAY,
    LOG_COLOR_WHITE,
    LOG_COLOR_BLUE,
    LOG_COLOR_YELLOW,
    LOG_COLOR_RED,
    LOG_COLOR_DARKRED,
    LOG_COLOR_RESET
};
// TODO: actual printf implementation in logger_kdefault.
// Probably would be a good idea to have it as a separate function logger_printf_base
static intptr_t logger_log_default(Logger* logger, uint32_t level, const char* fmt, va_list args) {
    if(level >= LOG_COUNT) {
        logger_log(logger, LOG_WARN, "Invalid log level %u >= %u", level, LOG_COUNT);
        return -UNSUPPORTED;
    }
    if(logger->draw_color) logger->draw_color(logger, color_map[level]);
    logger->write_str(logger, "[" , 1);
    const char* strlevel = str_map[level];
    logger->write_str(logger, strlevel, strlen(strlevel));
    logger->write_str(logger, "] ", 2);
    static char tmp_logger_buffer[1024];
    size_t n = stbsp_vsnprintf(tmp_logger_buffer, sizeof(tmp_logger_buffer), fmt, args);
    logger->write_str(logger, tmp_logger_buffer, n);
    if(logger->draw_color) logger->draw_color(logger, LOG_COLOR_RESET);
    logger->write_str(logger, "\n", 1);
    return 0;
}
intptr_t logger_init(Logger* logger) {
    if(!logger) return -INVALID_PARAM;
    if(!logger->log) {
        if(!logger->write_str) {
            return -UNSUPPORTED;
        }
        logger->log = logger_log_default;
    }
    return 0;
}
const char* ansi_map[] = {
    [LOG_COLOR_RESET]   = "\033[0m",
    [LOG_COLOR_GRAY]    = "\033[90m",
    [LOG_COLOR_WHITE]   = "\033[97m",
    [LOG_COLOR_BLUE]    = "\033[94m",
    [LOG_COLOR_YELLOW]  = "\033[93m",
    [LOG_COLOR_RED]     = "\033[91m",
    [LOG_COLOR_DARKRED] = "\033[31m",
};
static_assert(ARRAY_LEN(ansi_map) == LOG_COLOR_COUNT, "Update ansi_map");
const char* get_ansi_color_from_log(uint32_t color) {
    if(color >= ARRAY_LEN(ansi_map)) return NULL;
    return ansi_map[color];
}

#include "kernel.h"
#include "serial.h"
void init_loggers() {
    assert(logger_init(&serial_logger) == 0);
    kernel.logger = &serial_logger;
}
