
bool build_shell() {
    #define BINDIR "./bin/user/shell/"
    #define SRCDIR "./user/shell/src/"
    #define LIBDIR "./bin/std/"
    if(!cc_user    (SRCDIR "main.c"        , BINDIR "shell.o")) return false;
    Nob_File_Paths paths = {0};
    nob_da_append(&paths, BINDIR "shell.o");
    if(!find_objs(LIBDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_libc(&paths, true)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "shell"  , "./user/shell/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}
