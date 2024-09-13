#pragma once
#include "log.h"
#include "kernel.h"
#define BENCHBEGIN() size_t __bench_pit_ticks = kernel.pit_info.ticks
#define BENCHEND() kinfo("%s took %zums", __func__, kernel.pit_info.ticks-__bench_pit_ticks)
