#pragma once
#include <minos/sysstd.h>
#include <environ.h>
static intptr_t exec(const char* path, const char** argv, size_t argc) {
    return execve(path, argv, argc, (const char**)environ, __environ_size);
}
intptr_t execvp(const char* path, const char** argv, size_t argc);
