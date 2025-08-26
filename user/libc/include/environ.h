#pragma once
#include <stddef.h>
extern char** environ;
extern size_t __environ_size;
void _libc_init_environ(const char** envp);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
char* getenv(const char* name);
