#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"
typedef struct {
    File_Paths *dirs, *c_sources,
               *nasm_sources, *gen_sources;
} WalkDirOpt;


static bool walk_directory_opt(
    const char* path,
    const WalkDirOpt* opt
) {
    DIR *dir = opendir(path);
    if(!dir) {
        nob_log(NOB_ERROR, "Could not open directory %s: %s", path, strerror(errno));
        return false;
    }
    errno = 0;
    struct dirent *ent;
    while((ent = readdir(dir))) {
        if(*ent->d_name == '.') continue;
        size_t temp = nob_temp_save();
        const char* p = nob_temp_sprintf("%s/%s", path, ent->d_name); 
        String_View sv = sv_from_cstr(p);
        Nob_File_Type type = nob_get_file_type(p);
        if(type == NOB_FILE_DIRECTORY) {
            da_append(opt->dirs, p);
            if(!walk_directory_opt(p, opt)) {
                closedir(dir);
                return false;
            }
            continue;
        }
        if(opt->gen_sources && sv_end_with(sv, ".gen.c")) {
            nob_da_append(opt->gen_sources, p);
        } else if(opt->c_sources && sv_end_with(sv, ".c")) {
            nob_da_append(opt->c_sources, p);
        } else if(opt->nasm_sources && sv_end_with(sv, ".nasm")) {
            nob_da_append(opt->nasm_sources, p);
        } else {
            nob_temp_rewind(temp);
        }
    }
    closedir(dir);
    return true;
}

#define walk_directory(path, ...) walk_directory_opt(path, &(WalkDirOpt){__VA_ARGS__})

#define cstr_rem_suffix(__src, suffix) (int)(strlen(__src) - strlen(suffix)), (__src)

const char* inc_dirs[] = {
    "shared/include", 
    "src",
    "vendor/limine",
    "vendor/stb",
};
static void append_inc_dirs(Cmd* cmd) {
    for(size_t i = 0; i < ARRAY_LEN(inc_dirs); ++i) {
        cmd_append(cmd, "-I", inc_dirs[i]);
    }
}
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    char* cc = getenv("CC");
    if(!cc) cc = "cc";
    char* ld = getenv("LD");
    if(!ld) ld = "ld";
    char* bindir = getenv("BINDIR");
    if(!bindir) bindir = "bin";
    if(!nob_mkdir_if_not_exists_silent(bindir)) return 1;
    if(!nob_mkdir_if_not_exists_silent(nob_temp_sprintf("%s/kernel", bindir))) return 1;
    if(!nob_mkdir_if_not_exists_silent(nob_temp_sprintf("%s/iso", bindir))) return 1;
    if(!nob_mkdir_if_not_exists_silent(nob_temp_sprintf("%s/shared", bindir))) return 1;
    File_Paths dirs = { 0 };
    File_Paths c_sources = { 0 };
    File_Paths nasm_sources = { 0 };
    File_Paths gen_sources = { 0 }; 

    size_t src_dir = strlen("src/");
    if(!walk_directory("src", .dirs = &dirs, .c_sources = &c_sources, .nasm_sources = &nasm_sources, .gen_sources = &gen_sources)) return 1;
    File_Paths objs = { 0 };
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    for(size_t i = 0; i < dirs.count; ++i) {
        size_t temp = nob_temp_save();
        const char* dir = nob_temp_sprintf("%s/kernel/%s", bindir, dirs.items[i] + src_dir);
        if(!nob_mkdir_if_not_exists_silent(dir)) return 1;
        nob_temp_rewind(temp);
    }
    Cmd cmd = { 0 };
    for(size_t i = 0; i < gen_sources.count; ++i) {
        const char* src = gen_sources.items[i];
        const char* out = temp_sprintf("%s/kernel/%.*s", bindir, cstr_rem_suffix(src + src_dir, ".c"));
        const char* gen_file = temp_sprintf("%.*s", cstr_rem_suffix(src, ".gen.c"));
        bool needs_rebuild = nob_needs_rebuild1(gen_file, src);
        if(nob_c_needs_rebuild1(&stb, &pathb, out, src)) {
            cmd_append(&cmd, NOB_REBUILD_URSELF(out, src), "-O1", "-MMD");
            append_inc_dirs(&cmd);
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
        const char* out = nob_temp_sprintf("%s/kernel/%s.o", bindir, src + src_dir);
        const char* md = nob_temp_sprintf("%s/kernel/%s.d", bindir, src + src_dir);
        da_append(&objs, out);
        if(nob_c_needs_rebuild1(&stb, &pathb, out, src) == 0) continue;
        const char* include = nob_temp_sprintf("%.*s", (int)(nob_path_name(src)-src), src);
        cmd_append(&cmd, "nasm");
        cmd_append(&cmd, "-I", include); 
        cmd_append(&cmd, "-f", "elf64");
        cmd_append(&cmd, "-MD", md);
        cmd_append(&cmd, src);
        cmd_append(&cmd, "-o", out);
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = nob_temp_sprintf("%s/kernel/%s.o", bindir, src + src_dir);
        da_append(&objs, out);
        if(!nob_c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        cmd_append(&cmd, cc);
        cmd_append(&cmd, 
            "-g",
            "-nostdlib",
            "-march=x86-64",
            "-ffreestanding",
            "-static",
            "-Werror", "-Wno-unused-function",
            "-Wno-address-of-packed-member", 
            "-Wall", 
            "-fno-stack-protector", 
            "-fcf-protection=none", 
            "-O2",
            "-MMD",
            "-MP",
            /*"-fomit-frame-pointer", "-fno-builtin", */
            "-mgeneral-regs-only", 
            "-mno-mmx",
            "-mno-sse", "-mno-sse2",
            "-mno-3dnow",
            "-fPIC",
        );
        append_inc_dirs(&cmd);
        cmd_append(&cmd, "-c", src, "-o", out);
        if(!nob_cmd_run_sync_and_reset(&cmd)) {
            size_t temp = nob_temp_save();
            char* str = nob_temp_strdup(out);
            size_t str_len = strlen(str);
            assert(str_len);
            str[str_len-1] = 'd';
            nob_delete_file(str);
            nob_temp_rewind(temp);
            return 1;
        }
    }
    dirs.count = 0;
    c_sources.count = 0;
    nasm_sources.count = 0;
    gen_sources.count = 0;
    src_dir = strlen("shared/src/");
    if(!walk_directory("shared/src", .dirs = &dirs, .c_sources = &c_sources, .nasm_sources = &nasm_sources, .gen_sources = &gen_sources)) return 1;
    assert(dirs.count == 0 && "Update shared");
    assert(gen_sources.count == 0 && "Update shared");
    assert(nasm_sources.count == 0 && "Update shared");
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = nob_temp_sprintf("%s/shared/%.*s.o", bindir, cstr_rem_suffix(src + src_dir, ".c"));
        da_append(&objs, out);
        if(!nob_c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        cmd_append(&cmd, cc);
        cmd_append(&cmd, 
            "-g",
            "-nostdlib",
            "-march=x86-64",
            "-ffreestanding",
            "-static",
            "-Werror", "-Wno-unused-function",
            "-Wno-address-of-packed-member", 
            "-Wall", 
            "-fno-stack-protector", 
            "-fcf-protection=none", 
            "-O2",
            "-MMD",
            "-MP",
            /*"-fomit-frame-pointer", "-fno-builtin", */
            "-mgeneral-regs-only", 
            "-mno-mmx",
            "-mno-sse", "-mno-sse2",
            "-mno-3dnow",
            "-fPIC",
            "-I", "shared/include",
            "-Isrc",
        );
        cmd_append(&cmd, "-I", "vendor/limine");
        cmd_append(&cmd, "-I", "vendor/stb");
        cmd_append(&cmd, "-c", src, "-o", out);
        if(!nob_cmd_run_sync_and_reset(&cmd)) {
            size_t temp = nob_temp_save();
            char* str = nob_temp_strdup(out);
            size_t str_len = strlen(str);
            assert(str_len);
            str[str_len-1] = 'd';
            nob_delete_file(str);
            nob_temp_rewind(temp);
            return 1;
        }
    }
    const char* kernel = nob_temp_sprintf("%s/iso/kernel", bindir);
    if(nob_needs_rebuild(kernel, objs.items, objs.count)) {
        cmd_append(&cmd, ld, "-g", "-T", "linker.ld", "-o", kernel);
        da_append_many(&cmd, objs.items, objs.count);
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }
    const char* iso_files[] = {
        "vendor/limine/limine-bios.sys", 
        "vendor/limine/limine-bios-cd.bin",
        "vendor/limine/limine-uefi-cd.bin",
        "limine.cfg"
    };
    for(size_t i = 0; i < ARRAY_LEN(iso_files); ++i) {
        size_t temp = nob_temp_save();
        const char* out = nob_temp_sprintf("%s/iso/%s", bindir, path_name(iso_files[i]));
        if(nob_needs_rebuild1(out, iso_files[i]) && !nob_copy_file(iso_files[i], out)) return 1;
        nob_temp_rewind(temp);
    }
}
