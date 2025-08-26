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
        memcpy(new_environ, environ, __environ_size*sizeof(*environ));
        if(environ) free(environ);
        __environ_cap = ncap;
        environ = new_environ;
    }
    return true;
}
static int environ_find_index(const char* name) {
    if(name == NULL) return -1;
    size_t namelen = strlen(name);
    for(size_t i = 0; i < __environ_size; ++i) {
        char* at = strchr(environ[i], '=');
        if(at[0]=='\0') return -1;
        size_t e_namelen   = at-environ[i];
        if(namelen==e_namelen && memcmp(name, environ[i], namelen)==0) return i;
    }
    return -1;
}
static char* environ_constr(const char* name, const char* value) {
    size_t namelen = strlen(name);
    size_t valuelen = strlen(value);
    char* v = malloc(namelen+1+valuelen+1);
    if(!v) return v;
    memcpy(v, name, namelen);
    v[namelen]='=';
    memcpy(v+namelen+1, value, valuelen);
    v[namelen+1+valuelen]='\0';
    return v;
}
int setenv(const char* name, const char* value, int overwrite) {
    int i = environ_find_index(name);
    if(i < 0) {
        if(!__environ_reserve(1)) return -1;
        i = __environ_size++;
    } else if (!overwrite) return 0;
    char* env = environ_constr(name, value);
    if(!env) return -1;
    environ[i] = env;
    return 0;
}
int unsetenv(const char* name) {
    int i = environ_find_index(name);
    if(i < 0) return -1;
    memmove(environ+i, environ+i+1, __environ_size-i);
    __environ_size--;
    return 0;
}
char* getenv(const char* name) {
    int i = environ_find_index(name);
    if(i < 0) return NULL;
    char* at = strchr(environ[i], '=');
    if(at[0] == '\0') return NULL;
    return at+1;
}
void _libc_init_environ(const char** envp) {
    environ = NULL;
    __environ_cap = 0;
    __environ_size = 0;
    size_t envc = 0;
    while(envp[envc]) envc++;
    // Not enough memory
    if(!__environ_reserve(envc)) exit(1);
    for(size_t i = 0; i < envc; ++i) {
        char* env = strdup(envp[i]);
        if(!env) exit(1);
        environ[__environ_size++] = env;
    }
}
