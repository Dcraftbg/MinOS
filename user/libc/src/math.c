#include "math.h"
// TODO: Faster fabs in assembly
float fabs(float f) {
    return f < 0.0 ? -f : f;
}
#include <stdlib.h>
#include <stdio.h>
double pow(double x, double y) {
    fprintf(stderr, "TODO: pow");
    exit(1);
}

double ldexp(double x, int exponent) {
    double too=1.0;
    while(exponent > 0) {
        too *= 2.0;
        exponent--;
    }
    return x*too;
}
