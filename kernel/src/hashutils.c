#include "hashutils.h"
size_t dbj2(const char* str) {
    size_t hash = 5381;
    char c;
    while ((c = *str)) {
        hash = ((hash << 5) + hash) + c;
        str++;
    }
    return hash;
}
