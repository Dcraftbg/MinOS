// NOTE: Inspired by nob's da_append macros
#pragma once
#include <stdio.h>
#include <stdlib.h>
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
#define da_insert(da, index, value) \
   do {\
        assert((int)(index) <= (int)(da)->len && "Index out of bounds");\
        da_reserve(da, 1);\
        memmove(&(da)->items[(index) + 1], &(da)->items[(index)], ((da)->len - (index)) * sizeof(*(da)->items));\
        (da)->items[(index)] = (value);\
        (da)->len++;\
   } while(0)
