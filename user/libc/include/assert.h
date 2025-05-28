#pragma once
#include <stdlib.h>
#include <stdio.h>
#define __STRINGIFY2(x) #x
#define __STRINGIFY(x) __STRINGIFY2(x)
#define assert(x) \
    x ? 0 : (fputs(__FILE__ ":" __STRINGIFY(__LINE__) " Assertion failed: " __STRINGIFY(x) "\n", stderr), abort())

// TODO: the empty array trick for much older versions of C.
// I doubt we need it but I'm leaving it here just in case
// static char __static_assert ## __LINE__ [(cond)-1];
#if defined(__STDC_VERSION__)
#   if __STDC_VERSION__ >= 202311L
        // Since C23:
        // Do nothing, static_assert is already defined
#   elif __STDC_VERSION__ >= 201112L
#       define static_assert _Static_assert 
#   else
        // There is no static_assert :(
#       define static_assert(...)
#   endif 
#else
    // There is no static_assert :(
#   define static_assert(...)
#endif
