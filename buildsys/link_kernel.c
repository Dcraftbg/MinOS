bool link_kernel() {
    Nob_File_Paths paths = {0};
    if(!find_objs("./bin/kernel",&paths)) {
        nob_da_free(paths);
        return false;
    }

    if(!find_objs("./bin/std",&paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, "./bin/iso/kernel", "./linker/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    return true;
}

