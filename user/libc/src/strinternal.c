#include <strinternal.h>
#include <stdbool.h>
const char* strflip_internal(char* str, size_t len) {
    for(size_t i = 0; i < len/2; ++i) {
        char c = str[i];
        str[i] = str[len-i-1];
        str[len-i-1] = c;
    }
    return str;
}
size_t itoa_internal(char* buf, size_t cap, int value) {
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
    strflip_internal(whole, (buf+at)-whole);
    return at;
}
size_t sztoa_internal(char* buf, size_t cap, size_t value) {
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
    strflip_internal(buf, at);
    return at;
}
static const char* hex_digits = "0123456789ABCDEF";
size_t utoha_internal(char* buf, size_t cap, unsigned int value) {
    size_t at=0;
    while(at < cap && value > 0) {
        int k = value & 0xF;
        value >>= 4;
        buf[at++] = hex_digits[k];
    }
    strflip_internal(buf, at);
    return at;
}
size_t uptrtoha_full_internal(char* buf, size_t cap, uintptr_t value) {
    size_t at=0;
    while(at < cap && at < 16) {
        int k = value & 0xF;
        value >>= 4;
        buf[at++] = hex_digits[k];
    }
    strflip_internal(buf, at);
    return at;
}

size_t atosz_internal(const char* buf, const char** end) {
    size_t value=0;
    while(*buf && *buf >= '0' && *buf <= '9') {
        value *= 10;
        value += (*buf)-'0';
        buf++;
    }
    *end = buf;
    return value;
}

int atoi_internal(const char* buf, const char** end) {
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
