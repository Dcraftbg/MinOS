bool build_fbtest() {
    #define BINDIR "./bin/user/fbtest/"
    #define SRCDIR "./user/fbtest/src/"
    #define LIBDIR "./bin/std/"
    if(!build_user_dir(SRCDIR, BINDIR, false)) return false; 
    Nob_File_Paths paths = {0};
    if(!find_objs(BINDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_objs(LIBDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_libc(&paths, true)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "fbtest"  , "./user/fbtest/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}
