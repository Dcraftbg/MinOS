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
// TODO: Proper floor. This is not exactly correct as it doesn't account for sign
double floor(double f) {
    return (double)((unsigned long long)f);
}
#include <errno.h>
double fmod(double x, double y) {
    if (y == 0) {
        errno = EDOM;
        return x;
    }
    double q = x / y;
    double r = x - (y * floor(q));
    return r;
}

double frexp(double x, int* expptr) {
    fprintf(stderr, "TODO: frexp");
    exit(1);
}
