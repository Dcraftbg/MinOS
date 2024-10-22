bool build_hello() {
    #define BINDIR "./bin/user/hello/"
    #define SRCDIR "./user/hello/src/"
    #define LIBDIR "./bin/std/"
    if(!cc_user    (SRCDIR "main.c"        , BINDIR "hello.o")) return false;
    Nob_File_Paths paths = {0};
    nob_da_append(&paths, BINDIR "hello.o");
    if(!find_objs(LIBDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_libc(&paths, true)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "hello"  , "./user/hello/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}
