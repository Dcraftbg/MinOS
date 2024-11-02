#include "string.h"
#include "stdlib.h"
#include "ctype.h"
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

int strcasecmp(const char *restrict s1, const char *restrict s2) {
   while(*s1 && tolower(*s1) == tolower(*s2)) {
        s1++;
        s2++;
   }
   return ((unsigned char)*s1)-((unsigned char)*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t max) {
   while(max && *s1 && *s2 && (tolower(*s1) == tolower(*s2))) {
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
    return (char*)str;
}

char* strrchr(const char* str, int chr) {
    char* head=NULL;
    while(str[0]) {
        if(str[0] == (char)chr) head = (char*)str;
        str++;
    }
    return head;
}
char* strdup(const char* str) {
    size_t len = strlen(str);
    char* res = malloc(len+1);
    if(!res) goto end;
    memcpy(res, str, len+1);
end:
    return res;
}
char* strncpy(char *restrict dest, const char *restrict src, size_t len) {
    size_t src_len = strlen(src);
    if(len > src_len) len = src_len;
    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}
char* strstr (const char* str, const char* substr) {
    if(substr[0] == '\0') return (char*)str;
    size_t str_len = strlen(str);
    size_t substr_len = strlen(substr);
    while(str[0] && str_len > substr_len) {
        if(strcmp(str, substr) == 0) return (char*)str;
        str++;
        str_len--;
    }
    return NULL;
}
