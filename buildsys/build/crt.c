#include "crt.h"
bool build_crt0(void) {
    #define BINDIR LIBC_CRT_TARGET_DIR 
    #define SRCDIR "./user/libc/crt/"
    #define LIBDIR "./bin/std/"
    if(!build_user_dir(SRCDIR, BINDIR, false)) return false; 
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}

