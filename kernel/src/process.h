#pragma once
#include "list.h"
#include "page.h"
#include "resource.h"
#include <stddef.h>

typedef struct {
    size_t bytelen; // Calculated using argv[0..argc].sum(arg.len());
    size_t argc;
    const char** argv;
} Args;
static Args create_args(size_t argc, const char** argv) {
    size_t sum = 0;
    for(size_t i = 0; i < argc; ++i) {
        sum += strlen(argv[i])+1;
    }
    return (Args) { sum, argc, argv };
}

