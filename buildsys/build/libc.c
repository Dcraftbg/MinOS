#include "libc.h"
bool build_libc() {
    #define BINDIR LIBC_TARGET_DIR 
    #define SRCDIR "./user/libc/src/"
    #define LIBDIR "./bin/std/"
    if(!build_user_dir(SRCDIR, BINDIR, true)) return false; 
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}
