#pragma once
#include <stdint.h>
typedef intptr_t regoff_t;
// TODO: Implement regex
typedef struct {
    size_t re_nsub;
} regex_t;
typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;
#define REG_NOTBOL '^'
#define REG_NOTEOL '$'
