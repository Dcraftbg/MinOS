bool build_user(const char* what) {
    if(what) {
        if(strcmp(what, "libc") == 0) {
            if(!build_libc()) return false;
            if(!build_crt0()) return false;
        }
        else if(strcmp(what, "nothing") == 0) {
            if(!build_nothing()) return false;
        }
        else if(strcmp(what, "init") == 0) {
            if(!build_init()) return false;
        }
        else if(strcmp(what, "shell") == 0) {
            if(!build_shell()) return false;
        } 
        else if(strcmp(what, "hello") == 0) {
            if(!build_hello()) return false;
        }
        else if(strcmp(what, "cat") == 0) {
            if(!build_cat()) return false;
        }
        else if(strcmp(what, "ls") == 0) {
            if(!build_ls()) return false;
        }
        else if(strcmp(what, "fbtest") == 0) {
            if(!build_fbtest()) return false;
        } else {
            nob_log(NOB_ERROR, "user: Don't know how to build: `%s`", what);
            return false;
        }
    } else {
        if(!build_libc()) return false;
        if(!build_crt0()) return false;
        if(!build_nothing()) return false;
        if(!build_init()) return false;
        if(!build_shell()) return false;
        if(!build_hello()) return false;
        if(!build_cat()) return false;
        if(!build_ls()) return false;
        if(!build_fbtest()) return false;
    }
    return true; 
}
