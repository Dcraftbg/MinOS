#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {
    size_t secs, nano;
} MinOS_Duration;
typedef struct {
    uint64_t ms;
} MinOS_Time;
