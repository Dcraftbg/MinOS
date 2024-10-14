bool build_user() {
    if(!build_libc()) return false;
    if(!build_crt0()) return false;
    if(!build_nothing()) return false;
    if(!build_init()) return false;
    if(!build_shell()) return false;
    if(!build_hello()) return false;
    return true; 
}
