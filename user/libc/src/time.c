#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <minos/sysstd.h>
#include <assert.h>
time_t time(time_t* timer) {
    assert(timer == NULL && "timer doesn't support non NULL time yet");
    MinOS_Time time;
    gettime(&time);
    return time.ms / 1000;
}
double difftime(time_t time1, time_t time0) {
    return time1-time0;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    fprintf(stderr, "TODO: strftime is a stub");
    exit(1);
}

clock_t clock(void) {
    fprintf(stderr, "TODO: clock is a stub");
    exit(1);
}

time_t mktime(const struct tm* tm) {
    fprintf(stderr, "TODO: mktime is a stub");
    exit(1);
}
struct tm* localtime(const time_t* timer) {
    fprintf(stderr, "TODO: localtime is a stub");
    exit(1);
}
struct tm* gmtime(const time_t* timer) {
    fprintf(stderr, "TODO: gmtime is a stub");
    exit(1);
}
