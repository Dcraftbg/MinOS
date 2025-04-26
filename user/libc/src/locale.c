#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
char *setlocale(int category, const char *locale) {
    fprintf(stderr, "%s:%d: (WARN) setlocale is a stub\n", __FILE__, __LINE__);
    exit(1);
}
struct lconv *localeconv(void) {
    fprintf(stderr, "%s:%d: localeconv is a stub\n", __FILE__, __LINE__);
    exit(1);
}
