#include "nothing.h"
bool build_nothing() {
    #define BINDIR "./bin/user/nothing/"
    #define SRCDIR "./user/nothing/"
    if(!build_user_dir(SRCDIR, BINDIR, false)) return false; 
    Nob_File_Paths paths={0};
    if(!find_objs(BINDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "nothing"       , SRCDIR "link.ld")) return false;
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    return true;
}
