bool build_user(const char* what) {
    Nob_Cmd cmd = { 0 };
    if(what) {
        if(strcmp(what, "libc") == 0) {
            if(!build_libc()) return false;
            if(!build_crt0()) return false;
            return true;
        }
        const char* args[] = { "sysroot" };
        if(!go_run_nob_inside(&cmd, "user/toolchain", args, NOB_ARRAY_LEN(args))) return false;
        size_t temp = nob_temp_save();
        if(!go_run_nob_inside(&cmd, nob_temp_sprintf("user/%s", what), NULL, 0)) {
            nob_temp_rewind(temp);
            nob_cmd_free(cmd);
            return false;
        }
        nob_temp_rewind(temp);
    } else {
        if(!build_libc()) return false;
        if(!build_crt0()) return false;
        const char* args[] = { "sysroot" };
        if(!go_run_nob_inside(&cmd, "user/toolchain", args, NOB_ARRAY_LEN(args))) return false;
        // Always build the toolchain first 
        if(nob_file_exists("user/toolchain/bin/binutils/bin/x86_64-minos-gcc") != 1 && !go_run_nob_inside(&cmd, "user/toolchain", NULL, 0)) return false;
        {
            nob_cmd_append(&cmd, "x86_64-minos-gcc", "-v");
            if(!nob_cmd_run_sync_and_reset(&cmd)) {
                nob_log(NOB_WARNING, "Automatically adding toolchain to PATH...");
                size_t temp = nob_temp_save();
                const char* toolchain_path = nob_temp_realpath("user/toolchain/bin/binutils/bin/");
                if(!toolchain_path) return false;
                const char* path = getenv("PATH");
                if(!path) setenv("PATH", toolchain_path, 0);
                else {
                    char sep =
                    #if _WIN32
                        ';'
                    #else
                        ':'
                    #endif
                    ;
                    setenv("PATH", nob_temp_sprintf("%s%c%s", path, sep, toolchain_path), 1);
                }
                nob_temp_rewind(temp);
            }
        }
        Nob_File_Paths progs = { 0 };
        size_t temp = nob_temp_save();
        if(!nob_read_entire_dir("user", &progs)) return false;
        for(size_t i = 0; i < progs.count; ++i) {
            const char* name = progs.items[i];
            if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            // NOTE: List of ignored user programs.
            if(
                strcmp(name, "tinycc") == 0 || 
                strcmp(name, "toolchain") == 0 ||
                strcmp(name, "doodle") == 0
            ) {
                nob_log(NOB_INFO, "Skipping %s (Ignored)", name);
                continue; 
            }

            const char* path = nob_temp_sprintf("user/%s", name);
            if(nob_get_file_type(path) != NOB_FILE_DIRECTORY) continue;
            const char* nob = nob_temp_sprintf("%s/nob.c", path);
            if(nob_file_exists(nob) == 1) {
                nob_log(NOB_INFO, "Building %s", path);
                if(!go_run_nob_inside(&cmd, path, NULL, 0)) {
                    nob_log(NOB_ERROR, "Failed to build %s", path);
                    nob_temp_rewind(temp);
                    nob_cmd_free(cmd);
                    return false;
                }
            } else {
                nob_log(NOB_INFO, "Skipping %s (no nob.c)", path);
            }
        }
        nob_temp_rewind(temp);
        nob_da_free(progs);
    }
    nob_cmd_free(cmd);
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
bool go_run_nob_inside(Nob_Cmd* cmd, const char* dir, const char** argv, size_t argc) {
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
    nob_da_append_many(cmd, argv, argc);
    bool res = nob_cmd_run_sync_and_reset(cmd);
    assert(nob_set_current_dir(cur));
    nob_temp_rewind(temp);
    return res;
}
