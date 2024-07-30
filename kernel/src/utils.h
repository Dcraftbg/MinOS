#pragma once
#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(*arr))
#define PAGE_SIZE 4096
#define PAGE_ALIGN_DOWN(x) (((x) / PAGE_SIZE)*PAGE_SIZE)
#define PAGE_ALIGN_UP(x) ((((x) + (PAGE_SIZE-1))/ PAGE_SIZE)*PAGE_SIZE)
#define MAP_BLOCK_ALIGN(x) (((x) + 7)/8)
#define BREAKPOINT() asm volatile("xchgw %bx , %bx")
#define _STRING(x) #x
#define STRING(x) _STRING(x)
#ifdef __GNUC__
#define PRINTFLIKE(n,m) __attribute__((format(printf,n,m)))
#endif
#define BIT(n) (1 << (n-1))


#if defined(__STDC_VERSION__)
#   if __STDC_VERSION__ >= 202311L
        // Since C23:
        // Do nothing, static_assert is already defined
#   elif __STDC_VERSION__ >= 201112L
#       define static_assert _Static_assert 
#   else
#       if defined(STRIP_STATIC_ASSERT)
#           define static_assert(...)
#       else 
#           error ERROR: Cannot build with this old of a version of C. Please use C11 or later
#       endif
#   endif 
#else
#   if defined(STRIP_STATIC_ASSERT)
#       define static_assert(...)
#   else
#       error ERROR: Cannot build with this old of a version of C. Please use C11 or later
#   endif
#endif
void kabort();