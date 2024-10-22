#include "nothing.h"
bool build_nothing() {
    #define BINDIR "./bin/user/nothing/"
    #define SRCDIR "./user/nothing/"
    if(!nasm(SRCDIR "nothing.nasm", BINDIR "nothing.o")) return false;
    if(!simple_link(BINDIR "nothing.o"     , BINDIR "nothing"       , SRCDIR "link.ld")) return false;
    #undef BINDIR
    #undef SRCDIR
    return true;
}
