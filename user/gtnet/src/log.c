#include "log.h"
#include <stdarg.h>
const char* log_where;
void sink_log(FILE* sink, const char* mode, const char* where, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(sink, "%s:%s: ", mode, where);
    vfprintf(sink, fmt, args);
    fprintf(sink, NEWLINE);
    va_end(args);
}
