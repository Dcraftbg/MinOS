#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"
#define KLIBC 1
#include "../compiler_common.h"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    const char* cc = getenv_or_default("CC", "cc");
    const char* ld = getenv_or_default("LD", "ld");
    const char* ar = getenv_or_default("AR", "ar");
    const char* bindir = getenv_or_default("BINDIR", "bin");
    if(!mkdir_if_not_exists_silent(bindir)) return 1;
    if(!mkdir_if_not_exists_silent(temp_sprintf("%s/klibc", bindir))) return 1;

    Cmd cmd = { 0 };
    File_Paths c_sources = { 0 };

    size_t src_dir = strlen("src/");
    if(!walk_directory("src", { &c_sources, .ext = ".c" })) return 1;
    File_Paths objs = { 0 };
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = temp_sprintf("%s/klibc/%s.o", bindir, src + src_dir);
        da_append(&objs, out);
        if(!c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        kcc_common(&cmd);
        cmd_append(&cmd, "-c", src, "-o", out);
        kcc_run_and_cleanup(&cmd);
    }
    cmd_append(&cmd, ar, "cr", temp_sprintf("%s/libklibc.a", bindir));
    da_append_many(&cmd, objs.items, objs.count);
    return cmd_run_sync_and_reset(&cmd) ? 0 : 1;
}
