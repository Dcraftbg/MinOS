#define NOB_IMPLEMENTATION
#include "../nob.h"

#ifndef MINOS_ROOT
#   define MINOS_ROOT "../../../" /*Relative to the my_basic folder*/
#endif
#define EXE      "my_basic"
#if 1
#   define BUILD_DIR MINOS_ROOT "bin/" EXE "/"
#else
#   define BUILD_DIR "bin/"
#endif
#if 1
#   define COPY_DIR MINOS_ROOT "initrd/user/"
#endif
#define HASH "5aa0c1f9f7be71126ce415581f2fe7b26c425c3e"
#define URL  "https://github.com/paladin-t/my_basic.git"

#define c_compiler(cmd)     nob_cmd_append(cmd, "x86_64-minos-gcc")
#define c_output(cmd, path) nob_cmd_append(cmd, "-o", path)
#define c_flags(cmd)        nob_cmd_append(cmd, "-Wno-overflow", "-MD", "-g")

const char* patches[] = {
    "0001-Port-to-MinOS.patch"
};
bool do_patch(Nob_Cmd* cmd, const char* patch) {
    nob_cmd_append(cmd, "git", "apply", patch);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool do_clone(Nob_Cmd* cmd, const char* url) {
    nob_cmd_append(cmd, "git", "clone", url);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool do_checkout(Nob_Cmd* cmd, const char* hash) {
    nob_cmd_append(cmd, "git", "checkout", hash);
    return nob_cmd_run_sync_and_reset(cmd);
}
const char* sources[] = {
    "core/my_basic.c", "shell/main.c"
};
void git_cleanup_thingy(void) {
    fprintf(stderr, "IMPORTANT: In its current state the repository is invalid.\n");
    fprintf(stderr, "Please delete the directory so the next build will reclone it.\n");
    fprintf(stderr, "If not the port might be invalid and you'll have to manually fix things.\n");
}
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    const char* exe = nob_shift_args(&argc, &argv);
    Nob_Cmd cmd = { 0 };
    if(nob_file_exists("my_basic") != 1) {
        if(!do_clone(&cmd, URL)) {
            nob_log(NOB_ERROR, "Failed to clone repository `" URL "`");
            return 1;
        }
        assert(nob_set_current_dir("my_basic"));
        if(!do_checkout(&cmd, HASH)) { 
            nob_log(NOB_ERROR, "Failed to checkout hash `" HASH "`");
            git_cleanup_thingy();
            return 1;
        }
        for(size_t i = 0; i < NOB_ARRAY_LEN(patches); ++i) {
            size_t temp = nob_temp_save();
            const char* path = nob_temp_sprintf("../%s", patches[i]);
            if(!do_patch(&cmd, path)) {
                nob_log(NOB_ERROR, "Failed to apply patch Number %zu (%s).", i, patches[i]);
                git_cleanup_thingy();
                return 1;
            }
            nob_temp_rewind(temp);
        }
        assert(nob_set_current_dir(".."));
    }
    
    assert(nob_set_current_dir("my_basic"));

    if(!nob_mkdir_if_not_exists_silent(BUILD_DIR)) return 1;
    Nob_String_Builder sbuf = { 0 };
    Nob_File_Paths path_buf = { 0 };
    if(nob_c_needs_rebuild(&sbuf, &path_buf, BUILD_DIR EXE, sources, NOB_ARRAY_LEN(sources))) {
        c_compiler(&cmd);
        c_flags(&cmd);
        c_output(&cmd, BUILD_DIR EXE);
        nob_da_append_many(&cmd, sources, NOB_ARRAY_LEN(sources));
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }
#ifdef COPY_DIR
    if(nob_file_exists(COPY_DIR) == 1 && nob_needs_rebuild1(COPY_DIR EXE, BUILD_DIR EXE) && !nob_copy_file(BUILD_DIR EXE, COPY_DIR EXE)) return 1;
#endif
}
