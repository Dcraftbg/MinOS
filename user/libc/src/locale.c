#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
char *setlocale(int category, const char *locale) {
    fprintf(stderr, "ERROR: Todo. setlocale");
    exit(1);
}
struct lconv *localeconv(void) {
    fprintf(stderr, "ERROR: Todo. localeconv");
    exit(1);
}
