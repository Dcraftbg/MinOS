#include "serial.h"
#include "../../config.h"
#include "logger.h"
#define COM_PORT 0x3f8
#define COM_5 (COM_PORT+5)
#define COM_STATUS COM_5

#define COM_LINE_CONTROL_PORT (COM_PORT+3)
#define COM_FIFO_CONTROL_PORT (COM_PORT+2)
#define COM_INT_ENABLE_PORT   (COM_PORT+1)
void serial_init() {
    // Set baud rate (for example, 9600 baud)
    outb(COM_LINE_CONTROL_PORT, 0x80); // Enable DLAB (divisor latch access)
    outb(COM_PORT, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(COM_INT_ENABLE_PORT, 0x00); // Hi byte
    outb(COM_LINE_CONTROL_PORT, 0x03); // 8 bits, no parity, one stop bit
    outb(COM_FIFO_CONTROL_PORT, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(COM_INT_ENABLE_PORT, 0x00); // Enable received data available interrupt
}

static void serial_print_u8(uint8_t c) {
    while ((inb(COM_STATUS) & 0x20) == 0) {}
    outb(COM_PORT, c);
}
void serial_print_chr(uint32_t chr) {
    switch(chr) {
    case '\b':
        serial_print_u8('\b');
        serial_print_u8(' ');
        serial_print_u8('\b');
        break;
    default:
        serial_print_u8(chr);
        break;
    }
}
static inline bool data_avail() {
    return (inb(COM_STATUS) & 0x01) != 0; 
}
void serial_printstr(const char* str) {
    while(*str) {
        char c = *str++;
        serial_print_chr(c);
    }
}
void serial_print(const char* str, size_t len) {
    for(size_t i = 0; i < len; ++i) {
        serial_print_chr(str[i]);
    }
}


static intptr_t serial_log_write_str(Logger* this, const char* str, size_t len) {
    for(size_t i = 0; i < len; ++i) {
        serial_print_chr(str[i]);
    }
    return 0;
}
static intptr_t serial_log_draw_color(Logger* this, uint32_t color) {
    serial_printstr(get_ansi_color_from_log(color));
    return 0;
}

#include "print_base.h"

void serial_print_sink(void* _, const char* data, size_t len) { serial_print(data, len); }
static intptr_t serial_log(Logger* logger, uint32_t level, const char* fmt, va_list args) {
    if(level >= LOG_COUNT) return -UNSUPPORTED;
    #ifndef NO_SERIAL_COLOR
    serial_log_draw_color(logger, logger_color_map[level]);
    #endif
    {
        char buf[10];
        size_t at=0;
        buf[at++] = '[';
        const char* strlevel = logger_str_map[level];
        size_t len = strlen(strlevel);
        memcpy(buf+at, strlevel, len);
        at+=len;
        buf[at++] = ']';
        buf[at++] = ' ';
        serial_print(buf, at);
    }
    print_base(NULL, serial_print_sink, fmt, args);
    #ifndef NO_SERIAL_COLOR
    serial_log_draw_color(logger, LOG_COLOR_RESET);
    #endif
    serial_print_u8('\n');
    return 0;
}
#include "../../config.h"
Logger serial_logger = {
    .log = serial_log,
    .write_str = serial_log_write_str,
#ifdef NO_SERIAL_COLOR
    .draw_color = NULL,
#else
    .draw_color = serial_log_draw_color,
#endif
    .level = LOG_ALL,
    .private = NULL,
};
