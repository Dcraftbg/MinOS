#pragma once
float fabs(float f);
double pow(double x, double y);
double ldexp(double x, int exponent);
double floor(double a);
double fmod(double x, double y);
double frexp(double x, int* expptr);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double log(double x);
double log10(double x);
double log2(double x);
double exp(double x);
double sqrt(double x);
double ceil(double x);
double atan2(double y, double x);
#define INFINITY 1e5000f
#define HUGE_VALF INFINITY
#define HUGE_VAL  ((double)INFINITY)
#define HUGE_VALL ((long double)INFINITY)
