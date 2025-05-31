#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"


const char* projects[] = {
    "libc",
    "toolchain",
    // "libwm",
    "ansi_test",
    "cat",
    "dynld",
    "fbtest",
    "gtnet",
    "hello",
    "hello_window",
    "init",
    "libwm",
    "ls",
    "mimux",
    "mishualiser",
    "my_basic",
    "shell",
    "shm_test",
    "simpdump",
    "testy",
    // "tinycc",
    "wm",
};
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
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    for(size_t i = 0; i < ARRAY_LEN(projects); ++i) {
        nob_log(NOB_INFO, "Build %s", projects[i]);
        if(strcmp(projects[i], "toolchain") == 0) {
            const char* args[] = {
                "sysroot"
            };
            if(!go_run_nob_inside(&cmd, projects[i], args, ARRAY_LEN(args))) return 1;
            continue;
        }
        if(!go_run_nob_inside(&cmd, projects[i], NULL, 0)) return 1;
    }
}
