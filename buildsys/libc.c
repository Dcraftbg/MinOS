bool find_libc_core(Nob_File_Paths* paths) {
    return find_objs(LIBC_TARGET_DIR, paths);
}
bool find_libc_crt(Nob_File_Paths* paths) {
    return find_objs(LIBC_CRT_TARGET_DIR, paths);
}
bool find_libc(Nob_File_Paths* paths, bool with_crt) {
    if(!find_libc_core(paths)) return false;
    if(with_crt) 
        if (!find_libc_crt(paths)) return false;
    return true;
}
