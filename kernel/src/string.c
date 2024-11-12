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
const char* strflip(char* str, size_t len) {
    for(size_t i = 0; i < len/2; ++i) {
        char c = str[i];
        str[i] = str[len-i-1];
        str[len-i-1] = c;
    }
    return str;
}
size_t itoa(char* buf, size_t cap, int value) {
    size_t at=0;
    if(value == 0 && at < cap) {
        buf[at++] = '0';
        return at;
    }

    if(value < 0 && at < cap) {
        buf[at++] = '-';
        value = -value;
    }
    char* whole = buf+at;
    while(at < cap && value > 0) {
        int k = value % 10;
        value /= 10;
        buf[at++] = '0'+k;
    }
    strflip(whole, (buf+at)-whole);
    return at;
}
size_t sztoa(char* buf, size_t cap, size_t value) {
    size_t at=0;
    if(value == 0 && at < cap) {
        buf[at++] = '0';
        return at;
    }
    while(at < cap && value > 0) {
        int k = value % 10;
        value /= 10;
        buf[at++] = '0'+k;
    }
    strflip(buf, at);
    return at;
}
const char* hex_upper_digits = "0123456789ABCDEF";
const char* hex_lower_digits = "0123456789abcdef";
size_t utoha(char* buf, size_t cap, unsigned int value, const char* digits) {
    size_t at=0;
    while(at < cap && value > 0) {
        int k = value & 0xF;
        value >>= 4;
        buf[at++] = digits[k];
    }
    strflip(buf, at);
    return at;
}
size_t uptrtoha_full(char* buf, size_t cap, uintptr_t value, const char* digits) {
    size_t at=0;
    while(at < cap && at < 16) {
        int k = value & 0xF;
        value >>= 4;
        buf[at++] = digits[k];
    }
    strflip(buf, at);
    return at;
}

size_t atosz(const char* buf, const char** end) {
    size_t value=0;
    while(*buf && *buf >= '0' && *buf <= '9') {
        value *= 10;
        value += (*buf)-'0';
        buf++;
    }
    *end = buf;
    return value;
}

int atoi(const char* buf, const char** end) {
    bool neg = false;
    int value=0;
    if(*buf == '-') {
        neg = true;
        buf++;
    }
    while(*buf && *buf >= '0' && *buf <= '9') {
        value *= 10;
        value += (*buf)-'0';
        buf++;
    }
    *end = buf;
    return neg ? -value : value;
}
