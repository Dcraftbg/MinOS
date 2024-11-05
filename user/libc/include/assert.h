#pragma once
#include <stdlib.h>
#include <stdio.h>
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define assert(x) \
   do { \
      if(!(x)) { \
         fputs(__FILE__ ":" STRINGIFY(__LINE__) " Assertion failed: " STRINGIFY(x), stderr); \
         exit(1);\
      }\
   } while(0)

