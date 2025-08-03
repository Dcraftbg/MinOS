#pragma once
#include <stdio.h>
#define STRINGIFY0(x) # x
#define STRINGIFY1(x) STRINGIFY0(x)
#define stub(...) (fprintf(stderr, "TBD " __FILE__ ":"STRINGIFY1(__LINE__)": %s", __func__), fprintf(stderr, " " __VA_ARGS__), fprintf(stderr, "\n"))
