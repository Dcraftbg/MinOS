#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"


const char* projects[] = {
    // "libwm",
    "ansi_test",
    "cat",
    "dynld",
    "fbtest",
    "gtnet",
    "hello",
    "hello_window",
    "init",
    "ls",
    "mimux",
    "my_basic",
    "shell",
    "shm_test",
    "simpdump",
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

    // Always build libc first
    if(!go_run_nob_inside(&cmd, "libc", NULL, 0)) return 1;
    if(file_exists("toolchain/bin/binutils/bin/x86_64-minos-gcc") != 1) {
        char* cc = getenv("CC");
        char* ld = getenv("LD");
        unsetenv("CC");
        unsetenv("LD");
        if(!go_run_nob_inside(&cmd, "toolchain", NULL, 0)) return 1;
        setenv("CC", cc, 0);
        setenv("LD", ld, 0);
    }
    // Then update the toolchain sysroot
    {
        const char* args[] = {
            "sysroot"
        };
        if(!go_run_nob_inside(&cmd, "toolchain", args, ARRAY_LEN(args))) return 1;
    }
    // Then check for x86_64-minos-gcc in PATH
    cmd_append(&cmd, "x86_64-minos-gcc", "-v");
    if(!cmd_run_sync_and_reset(&cmd)) {
        nob_log(NOB_WARNING, "Automatically adding toolchain to PATH...");
        size_t temp = nob_temp_save();
        const char* toolchain_path = nob_temp_realpath("toolchain/bin/binutils/bin/");
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
    for(size_t i = 0; i < ARRAY_LEN(projects); ++i) {
        nob_log(NOB_INFO, "Build %s", projects[i]);
        if(!go_run_nob_inside(&cmd, projects[i], NULL, 0)) return 1;
    }
}
