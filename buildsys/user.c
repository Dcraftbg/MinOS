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
        else if(strcmp(what, "ansi_test") == 0) {
            if(!build_ansi_test()) return false;
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
        if(!build_ansi_test()) return false;
        if(!build_ls()) return false;
        if(!build_fbtest()) return false;
    }
    return true; 
}
// bool go_run_inside(Nob_Cmd* cmd, const char* dir) {
//     const char* cur = nob_get_current_dir_temp();
//     assert(cur);
//     cur = nob_temp_realpath(cur);
//     if(!nob_set_current_dir(dir)) return false;
//     if(!nob_set_current_dir(dir)) return false;
//     bool res = nob_cmd_run_sync_and_reset(cmd);
//     assert(nob_set_current_dir(cur));
//     return res;
// }

// TODO: cleanup temp on failure
bool go_run_nob_inside(Nob_Cmd* cmd, const char* dir) {
    size_t temp = nob_temp_save();
    const char* cur = nob_get_current_dir_temp();
    assert(cur);
    cur = nob_temp_realpath(cur);
    if(!nob_set_current_dir(dir)) return false;
    if(nob_file_exists("nob") != 1) {
        nob_cmd_append(cmd, NOB_REBUILD_URSELF("nob", "nob.c"));
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
    nob_cmd_append(cmd, "./nob");
    bool res = nob_cmd_run_sync_and_reset(cmd);
    assert(nob_set_current_dir(cur));
    nob_temp_rewind(temp);
    return res;
}
