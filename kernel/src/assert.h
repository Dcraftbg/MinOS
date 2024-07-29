#include "serial.h"
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define assert(x) \
   do { \
      if(!(x)) { \
         serial_printstr(__FILE__ ":" STRINGIFY(__LINE__) " Assertion failed: " STRINGIFY(x)); \
         for(;;) asm volatile("hlt");\
      }\
   } while(0)

#define debug_assert(x) assert(x)
