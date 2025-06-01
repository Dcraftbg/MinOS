#define NOB_IMPLEMENTATION
#include "../nob.h"

#ifndef MINOS_ROOT
#   define MINOS_ROOT "../../"
#endif
#define EXE      "wm"
#define BUILD_DIR MINOS_ROOT "bin/" EXE "/"
#define INITRD    MINOS_ROOT "initrd/"
#if 1
#   define COPY_DIR INITRD "user/"
#   define COPY_RES INITRD "res"
#endif

#define c_compiler(cmd)     nob_cmd_append(cmd, "x86_64-minos-gcc")
#define c_output(cmd, path) nob_cmd_append(cmd, "-o", path)
#define c_flags(cmd)        nob_cmd_append(cmd, "-Wall", "-Wextra", "-Wno-unused-function", "-Wno-unused-variable", "-MD", "-g")


const char* sources[] = {
    "src/main.c",
};
#define sources_count NOB_ARRAY_LEN(sources)
bool build_main(Nob_String_Builder* sbuf, Nob_File_Paths* path_buf, Nob_Cmd* cmd) {
    if(!nob_c_needs_rebuild(sbuf, path_buf, BUILD_DIR EXE, sources, sources_count)) return true;
    c_compiler(cmd);
    c_flags(cmd);
    nob_cmd_append(cmd, "-Werror");
    nob_cmd_append(cmd, "-O2");
    nob_cmd_append(cmd, BUILD_DIR "stb_image.o");
    nob_cmd_append(cmd, "-L" MINOS_ROOT "bin/libwm");
    nob_cmd_append(cmd, "-lwm");
    nob_cmd_append(cmd, "-I../libwm/include");
    nob_da_append_many(cmd, sources, sources_count);
    nob_cmd_append(cmd, "-Ivendor/stb");
    c_output(cmd, BUILD_DIR EXE);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool build_vendor(Nob_String_Builder* sbuf, Nob_File_Paths* path_buf, Nob_Cmd* cmd) {
    if(!nob_c_needs_rebuild1(sbuf, path_buf, BUILD_DIR "stb_image.o", "vendor/stb/stb_image.c")) return true;
    c_compiler(cmd);
    c_flags(cmd);
    nob_cmd_append(cmd, "-Ivendor/stb");
    nob_cmd_append(cmd, "-c");
    nob_cmd_append(cmd, "vendor/stb/stb_image.c");
    c_output(cmd, BUILD_DIR "stb_image.o");
    return nob_cmd_run_sync_and_reset(cmd);
}
bool copy_exe(void) {
#ifdef COPY_DIR
    if(nob_file_exists(COPY_DIR) != 1) return true; 
    if(!nob_needs_rebuild1(COPY_DIR EXE, BUILD_DIR EXE)) return true;
    return nob_copy_file(BUILD_DIR EXE, COPY_DIR EXE);
#else
    return true;
#endif
}
bool copy_resources(void) {
#ifdef COPY_RES
    return nob_copy_need_update_directory_recursively("res", COPY_RES);
#else
    return true;
#endif
}
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    const char* exe = nob_shift_args(&argc, &argv);
    if(!nob_mkdir_if_not_exists_silent(BUILD_DIR)) return 1;
    if(!nob_mkdir_if_not_exists_silent(INITRD "sockets/")) return 1;
    Nob_String_Builder sbuf = { 0 };
    Nob_File_Paths path_buf = { 0 };
    if(!build_vendor(&sbuf, &path_buf, &cmd)) return 1;
    if(!build_main(&sbuf, &path_buf, &cmd)) return 1;
    if(!copy_exe()) return 1;
    if(!copy_resources()) return 1;
    return 0;
}


