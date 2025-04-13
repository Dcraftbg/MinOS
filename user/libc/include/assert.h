#pragma once
#include <stdlib.h>
#include <stdio.h>
#define __STRINGIFY2(x) #x
#define __STRINGIFY(x) __STRINGIFY2(x)
#define assert(x) \
    x ? 0 : (fputs(__FILE__ ":" __STRINGIFY(__LINE__) " Assertion failed: " __STRINGIFY(x), stderr), exit(1))

