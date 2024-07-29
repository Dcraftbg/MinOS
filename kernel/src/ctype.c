#include "ctype.h"
// TODO: use bitmap lookup
static const char* isprint_chars = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
int isprint(int c) {
    const char* head = isprint_chars;
    while(*head) {
        if(c==*head) return 1;
        head++;
    }
    return 0;
}
