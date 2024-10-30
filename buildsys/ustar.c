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
    if(!copy_all_to(
        "./initrd/user",
        "./bin/user/nothing/nothing",
        "./bin/user/init/init",
        "./bin/user/shell/shell",
        "./bin/user/hello/hello",
        "./bin/user/fbtest/fbtest",
    )) return false;
    if(!ustar_zip_dir("./initrd", "./bin/iso/initrd")) return false;
    return true;
}
