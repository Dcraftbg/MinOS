#include "ustar.h"
bool ustar_zip_dir(const char* dir, const char* result) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "tar", 
        "--create", nob_temp_sprintf("--file=%s", result),
        "--format=ustar",
        "-C", dir,
        "."
    );
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool initrd_setup() {
    const char* doom_path = "./user/doomgeneric/doomgeneric/doomgeneric";
    if(nob_file_exists(doom_path) == 1) {
        if(!copy_all_to(
            "./initrd/user",
            doom_path
        )) return false;
    } else {
        nob_log(NOB_WARNING, "Could not find `%s`. Won't be embedded", doom_path);
    }
    if(!ustar_zip_dir("./initrd", "./bin/iso/initrd")) return false;
    return true;
}
