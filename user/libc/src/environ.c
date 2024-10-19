#include "environ.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
char** environ;
size_t __environ_cap;
size_t __environ_size;
static bool __environ_reserve(size_t extra) {
    if(__environ_size+extra > __environ_cap) {
        size_t ncap = __environ_cap*2+extra;
        char** new_environ = malloc(ncap*sizeof(*environ));
        if(!new_environ) return false;
        memcpy(new_environ, environ, __environ_size);
        __environ_cap = ncap;
        free(environ);
    }
    return true;
}
static char* __strdup(const char* str) {
    size_t len = strlen(str);
    char* res = malloc(len+1);
    if(!res) goto end;
    memcpy(res, str, len+1);
end:
    return res;
}
void _libc_init_environ(const char** envv, int envc) {
    environ = NULL;
    __environ_cap = 0;
    __environ_size = 0;
    // Not enough memory
    if(!__environ_reserve(envc)) exit(1);
    for(size_t i = 0; i < envc; ++i) {
        char* env = __strdup(envv[i]);
        if(!env) exit(1);
        environ[__environ_size++] = env;
    }
}
