#pragma once
#define MAX_PARAMS 64
#include <stdbool.h>
char* cmdline_get(const char* name);
bool  cmdline_set(const char* name, char* value);
void init_cmdline();
