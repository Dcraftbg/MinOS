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

double sin(double x) {
    fprintf(stderr, "TODO: sin(double x) is a stub");
    exit(1);
}
double cos(double x) {
    fprintf(stderr, "TODO: cos(double x) is a stub");
    exit(1);
}
double tan(double x) {
    fprintf(stderr, "TODO: tan(double x) is a stub");
    exit(1);
}
double asin(double x) {
    fprintf(stderr, "TODO: asin(double x) is a stub");
    exit(1);
}
double acos(double x) {
    fprintf(stderr, "TODO: acos(double x) is a stub");
    exit(1);
}
double atan(double x) {
    fprintf(stderr, "TODO: atan(double x) is a stub");
    exit(1);
}
double sinh(double x) {
    fprintf(stderr, "TODO: sinh(double x) is a stub");
    exit(1);
}
double cosh(double x) {
    fprintf(stderr, "TODO: cosh(double x) is a stub");
    exit(1);
}
double tanh(double x) {
    fprintf(stderr, "TODO: tanh(double x) is a stub");
    exit(1);
}
double log(double x) {
    fprintf(stderr, "TODO: log(double x) is a stub");
    exit(1);
}
double log10(double x) {
    fprintf(stderr, "TODO: log10(double x) is a stub");
    exit(1);
}
double log2(double x) {
    fprintf(stderr, "TODO: log2(double x) is a stub");
    exit(1);
}
double exp(double x) {
    fprintf(stderr, "TODO: exp(double x) is a stub");
    exit(1);
}
double sqrt(double x) {
    fprintf(stderr, "TODO: sqrt(double x) is a stub");
    exit(1);
}
double ceil(double x) {
    fprintf(stderr, "TODO: ceil(double x) is a stub");
    exit(1);
}
double atan2(double y, double x) {
    fprintf(stderr, "TODO: atan2(double y, double x) is a stub");
    exit(1);
}
