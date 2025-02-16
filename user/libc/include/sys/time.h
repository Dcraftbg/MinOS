#pragma once
#include <time.h>
#include <stdint.h>
typedef uint64_t suseconds_t;
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
