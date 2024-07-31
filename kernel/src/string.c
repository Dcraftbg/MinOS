#include "memory.h"

void *memset (void *dest, int x          , size_t n) {
    asm volatile(
        "rep stosb"
        : "=D"(dest), "=c"(n)
        : "D"(dest), "a"(x), "c"(n)
        : "memory"
    );
    return dest;
}
void *memcpy (void *dest, const void *src, size_t n) {
    asm volatile(
        "rep movsb"
        : "=D"(dest), "=S"(src), "=c"(n)
        : "D"(dest), "S"(src), "c"(n)
        : "memory"
    );
    return dest;
}
void *memmove(void* dest, const void* src, size_t n) {
    if (dest < src) {
        asm volatile(
            "rep movsb"
            : "=D"(dest), "=S"(src), "=c"(n)
            : "D"(dest), "S"(src), "c"(n)
            : "memory"
        );
    } else {
        // Calculate the end pointers for reverse copy
        dest = (char *)dest + n - 1;
        src = (const char *)src + n - 1;
        asm volatile(
            "std\n\t"            // Set direction flag to decrement
            "rep movsb\n\t"
            "cld"                // Clear direction flag to increment
            : "=D"(dest), "=S"(src), "=c"(n)
            : "D"(dest), "S"(src), "c"(n)
            : "memory"
        );
    }
    return dest;
}
// TODO: Optimise with rep
int strcmp(const char *restrict s1, const char *restrict s2) {
   while(*s1 && *s1 == *s2) {
        s1++;
        s2++;
   }
   return ((unsigned char)*s1)-((unsigned char)*s2);
}

int memcmp(const char* s1, const char* s2, size_t max) {
   while(max && (*s1 == *s2)) {
        s1++;
        s2++;
        max--;
   }
   if(max != 0) return ((unsigned char)*s1) - ((unsigned char)*s2);
   return 0;
}
int strncmp(const char* s1, const char* s2, size_t max) {
   while(max && *s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
        max--;
   }
   if(max != 0) return ((unsigned char)*s1) - ((unsigned char)*s2);
   return 0;
}
size_t strlen(const char* cstr) {
    const char* head = cstr;
    while(*head) head++;
    return head-cstr;
}
