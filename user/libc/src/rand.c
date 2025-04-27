#include "rand.h"
#include <stdio.h>
#include <stdlib.h>
int rand(void) {
    fprintf(stderr, "rand is a stub\n");
    exit(1);
}
void srand(unsigned int seed) {
    fprintf(stderr, "srand is a stub\n");
    exit(1);
}
