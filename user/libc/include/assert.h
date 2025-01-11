#pragma once
#include <stdlib.h>
#include <stdio.h>
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define assert(x) \
    x ? 0 : (fputs(__FILE__ ":" STRINGIFY(__LINE__) " Assertion failed: " STRINGIFY(x), stderr), exit(1))

