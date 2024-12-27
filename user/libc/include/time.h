#pragma once
#include <stddef.h>
typedef size_t time_t;
struct tm {
    int tm_sec,
        tm_min,
        tm_hour,
        tm_mday,
        tm_mon,
        tm_year,
        tm_wday,
        tm_yday,
        tm_isdst;
};
time_t mktime(const struct tm* tm);
struct tm* localtime(const time_t* timer);
struct tm* gmtime(const time_t* timer);
time_t time(time_t* timer);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
double difftime(time_t time1, time_t time0);
#define CLOCKS_PER_SEC 1000000
typedef size_t clock_t;
clock_t clock(void);
