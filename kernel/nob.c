#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"
#include "compiler_common.h"

#define cstr_rem_suffix(__src, suffix) (int)(strlen(__src) - strlen(suffix)), (__src)
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    const char* cc = getenv_or_default("CC", "cc");
    const char* ld = getenv_or_default("LD", "ld");
    const char* bindir = getenv_or_default("BINDIR", "bin");
    if(!mkdir_if_not_exists_silent(bindir)) return 1;
    if(!mkdir_if_not_exists_silent(temp_sprintf("%s/kernel", bindir))) return 1;
    if(!mkdir_if_not_exists_silent(temp_sprintf("%s/iso", bindir))) return 1;
    if(!mkdir_if_not_exists_silent(temp_sprintf("%s/shared", bindir))) return 1;

    Cmd cmd = { 0 };
    if(!go_run_nob_inside(&cmd, "klibc")) return 1;
    File_Paths dirs = { 0 };
    File_Paths c_sources = { 0 };
    File_Paths nasm_sources = { 0 };
    File_Paths gen_sources = { 0 }; 

    size_t src_dir = strlen("src/");
    if(!walk_directory("src",
            { &dirs, .file_type = FILE_DIRECTORY },
            { &gen_sources, .ext = ".gen.c" },
            { &c_sources, .ext = ".c" },
            { &nasm_sources, .ext = ".nasm" }
        )) return 1;
    File_Paths objs = { 0 };
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    for(size_t i = 0; i < dirs.count; ++i) {
        size_t temp = temp_save();
        const char* dir = temp_sprintf("%s/kernel/%s", bindir, dirs.items[i] + src_dir);
        if(!mkdir_if_not_exists_silent(dir)) return 1;
        temp_rewind(temp);
    }
    for(size_t i = 0; i < gen_sources.count; ++i) {
        const char* src = gen_sources.items[i];
        const char* out = temp_sprintf("%s/kernel/%.*s", bindir, cstr_rem_suffix(src + src_dir, ".c"));
        const char* gen_file = temp_sprintf("%.*s", cstr_rem_suffix(src, ".gen.c"));
        bool needs_rebuild = needs_rebuild1(gen_file, src);
        if(c_needs_rebuild1(&stb, &pathb, out, src)) {
            cmd_append(&cmd, NOB_REBUILD_URSELF(out, src), "-O1", "-MMD", KCC_INCLUDE_DIRS);
            if(!cmd_run_sync_and_reset(&cmd)) return 1;
            needs_rebuild = true;
        }
        if(!needs_rebuild) continue;
        nob_log(NOB_INFO, "Regenerating %s", gen_file);
        Fd fd = fd_open_for_write(gen_file);
        if(fd == -NOB_INVALID_FD) {
            nob_log(NOB_ERROR, "Failed to open %s", gen_file);
        }
        cmd_append(&cmd, out);
        // TODO: run in that directory
        if(!cmd_run_sync_redirect_and_reset(&cmd, (Cmd_Redirect){ .fdout = &fd })) {
            nob_log(NOB_ERROR, "FAILED to generate %s (using %s)", gen_file, src);
            fd_close(fd);
        }
        fd_close(fd);
    }

    for(size_t i = 0; i < nasm_sources.count; ++i) {
        const char* src = nasm_sources.items[i];
        const char* out = temp_sprintf("%s/kernel/%s.o", bindir, src + src_dir);
        const char* md = temp_sprintf("%s/kernel/%s.d", bindir, src + src_dir);
        da_append(&objs, out);
        if(c_needs_rebuild1(&stb, &pathb, out, src) == 0) continue;
        const char* include = temp_sprintf("%.*s", (int)(path_name(src)-src), src);
        cmd_append(&cmd, "nasm");
        cmd_append(&cmd, "-I", include); 
        cmd_append(&cmd, "-f", "elf64");
        cmd_append(&cmd, "-MD", md);
        cmd_append(&cmd, src);
        cmd_append(&cmd, "-o", out);
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = temp_sprintf("%s/kernel/%s.o", bindir, src + src_dir);
        da_append(&objs, out);
        if(!c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        kcc_common(&cmd);
        cmd_append(&cmd, "-c", src, "-o", out);
        kcc_run_and_cleanup(&cmd);
    }
    dirs.count = c_sources.count = nasm_sources.count = gen_sources.count = 0;
    src_dir = strlen("shared/src/");
    if(!walk_directory("shared/src", { &c_sources, .ext = ".c" })) return 1;
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = temp_sprintf("%s/shared/%.*s.o", bindir, cstr_rem_suffix(src + src_dir, ".c"));
        da_append(&objs, out);
        if(!c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        kcc_common(&cmd);
        cmd_append(&cmd, "-c", src, "-o", out);
        kcc_run_and_cleanup(&cmd);
    }
    const char* kernel = temp_sprintf("%s/iso/kernel", bindir);
    if(needs_rebuild(kernel, objs.items, objs.count)) {
        cmd_append(&cmd, ld, "-g", "-L", bindir, "-lklibc", "-T", "linker.ld", "-o", kernel);
        da_append_many(&cmd, objs.items, objs.count);
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    const char* iso_files[] = {
        "vendor/limine/limine-bios.sys", 
        "vendor/limine/limine-bios-cd.bin",
        "vendor/limine/limine-uefi-cd.bin",
        "limine.cfg"
    };
    for(size_t i = 0; i < ARRAY_LEN(iso_files); ++i) {
        size_t temp = temp_save();
        const char* out = temp_sprintf("%s/iso/%s", bindir, path_name(iso_files[i]));
        if(!copy_if_needed(iso_files[i], out)) return 1;
        temp_rewind(temp);
    }
}
