#pragma once
#include <minos/sysstd.h>
static intptr_t exec(const char* path, const char** argv, size_t argc) {
    return execve(path, argv, argc, (const char**)0, 0);
}
