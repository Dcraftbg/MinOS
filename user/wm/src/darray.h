// NOTE: Inspired by nob's da_append macros
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define DA_REALLOC(optr, osize, new_size) realloc(optr, new_size)
#define da_reserve(da, extra) \
   do {\
      if((da)->len + extra >= (da)->cap) {\
          void* _da_old_ptr;\
          size_t _da_old_cap = (da)->cap;\
          (void)_da_old_cap;\
          (void)_da_old_ptr;\
          (da)->cap = (da)->cap*2+extra;\
          _da_old_ptr = (da)->items;\
          (da)->items = DA_REALLOC(_da_old_ptr, _da_old_cap*sizeof(*(da)->items), (da)->cap*sizeof(*(da)->items));\
          assert((da)->items && "Ran out of memory");\
      }\
   } while(0)
#define da_push(da, value) \
   do {\
        da_reserve(da, 1);\
        (da)->items[(da)->len++]=value;\
   } while(0)

#define da_remove_unordered(da, n) \
   (da)->items[(n)] = (da)->items[--(da)->len]

#define da_zero(da) \
   memset((da)->items, 0, (da)->len * sizeof((da)->items[0]))
