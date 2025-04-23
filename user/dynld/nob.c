#define NOB_IMPLEMENTATION
#include "../nob.h"

#ifndef MINOS_ROOT
#   define MINOS_ROOT "../../"
#endif
#define EXE      "dynld"
#define BUILD_DIR MINOS_ROOT "bin/" EXE "/"
#if 1
#   define COPY_DIR MINOS_ROOT "initrd/user/"
#endif

#define c_compiler(cmd)     nob_cmd_append(cmd, "x86_64-minos-gcc")
#define c_output(cmd, path) nob_cmd_append(cmd, "-o", path)
#define c_warns(cmd)        nob_cmd_append(cmd, "-Wall", "-Wextra", "-Werror", "-Wno-unused-function")
#define c_env(cmd)          nob_cmd_append(cmd, "-ffunction-sections", "-fdata-sections", "-mgeneral-regs-only")
#define c_pie(cmd)          nob_cmd_append(cmd, "-pie", "-fpie")
#define c_standalone(cmd)   nob_cmd_append(cmd, "-nostdlib", "-nolibc", "-ffreestanding")
#define c_flags(cmd)        nob_cmd_append(cmd, "-static", "-g", "-MD")


const char* sources[] = {
    "src/main.c", "src/printf.c", "src/string.c", "src/strinternal.c"
};

bool cc(Nob_Cmd* cmd) {
    c_compiler(cmd);
    c_flags(cmd);
    c_warns(cmd);
    c_env(cmd);
    c_pie(cmd);
    c_standalone(cmd);
    nob_da_append_many(cmd, sources, NOB_ARRAY_LEN(sources));
    c_output(cmd, BUILD_DIR EXE);
    return nob_cmd_run_sync_and_reset(cmd);
}
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    const char* exe = nob_shift_args(&argc, &argv);
    if(!nob_mkdir_if_not_exists_silent(BUILD_DIR)) return 1;
    Nob_String_Builder sbuf = { 0 };
    Nob_File_Paths path_buf = { 0 };
    if(nob_c_needs_rebuild(&sbuf, &path_buf, BUILD_DIR EXE, sources, NOB_ARRAY_LEN(sources)) && !cc(&cmd)) return 1;
#ifdef COPY_DIR
    if(nob_file_exists(COPY_DIR) == 1 && nob_needs_rebuild1(COPY_DIR EXE, BUILD_DIR EXE) && !nob_copy_file(BUILD_DIR EXE, COPY_DIR EXE)) return 1;
#endif
    return 0;
}


