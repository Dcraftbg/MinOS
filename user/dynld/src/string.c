#include "string.h"
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

void *memchr (const void* ptr, int chr, size_t count) {
    while(count && ((const char*)ptr)[0] != (char)chr) {
        ptr++;
        count--;
    }
    return count ? (void*)ptr : NULL;
}
// TODO: Optimise with rep
int strcmp(const char *restrict s1, const char *restrict s2) {
   while(*s1 && *s1 == *s2) {
        s1++;
        s2++;
   }
   return ((unsigned char)*s1)-((unsigned char)*s2);
}

int memcmp(const void* s1_void, const void* s2_void, size_t max) {
   const char* s1 = (const char*)s1_void;
   const char* s2 = (const char*)s2_void;
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

char* strchr(const char* str, int chr) {
    while(str[0] && str[0] != (char)chr) str++;
    return str[0] ? (char*)str : NULL;
}

char* strrchr(const char* str, int chr) {
    char* head=NULL;
    while(str[0]) {
        if(str[0] == (char)chr) head = (char*)str;
        str++;
    }
    return head;
}
char* strpbrk(const char* str, const char* breakset) {
    while(str[0]) {
        for(const char* bs=breakset; bs[0]; bs++) {
            if(str[0] == bs[0]) return (char*)str;
        }
        str++;
    }
    return NULL;
}
// NOTE: Taken from the man page for strncpy
char* strncpy(char* dest, const char* src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
         dest[i] = src[i];
     for ( ; i < n; i++)
         dest[i] = '\0';
    return dest;
}
char* strstr(const char* str, const char* substr) {
    if(substr[0] == '\0') return (char*)str;
    size_t str_len = strlen(str);
    size_t substr_len = strlen(substr);
    while(str[0] && str_len > substr_len) {
        if(memcmp(str, substr, substr_len) == 0) return (char*)str;
        str++;
        str_len--;
    }
    return NULL;
}
