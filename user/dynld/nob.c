#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../../nob.h"
static bool walk_directory(
    File_Paths* dirs,
    File_Paths* c_sources,
    const char* path
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
            da_append(dirs, p);
            if(!walk_directory(dirs, c_sources, p)) {
                closedir(dir);
                return false;
            }
            continue;
        }
        if(sv_end_with(sv, ".c")) {
            nob_da_append(c_sources, p);
        } else {
            nob_temp_rewind(temp);
        }
    }
    closedir(dir);
    return true;
}
const char* get_or_default_set_default(const char* env, const char* default_value, bool set_default) {
    const char* v = getenv(env);
    // override the value if its not set
    if(!v) {
        v = default_value;
        if(set_default) setenv(env, v, 0);
    }
    return v;
}

typedef struct { bool set_default; } GetOrDefaultSetDefault;
#define get_or_default(env, default_value, ...) get_or_default_set_default(env, default_value, (GetOrDefaultSetDefault){__VA_ARGS__}.set_default) 
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    const char* bindir = get_or_default("BINDIR", "bin");
    const char* cc = get_or_default("CC", "cc");

    if(!mkdir_if_not_exists(bindir)) return 1;
    if(!mkdir_if_not_exists(temp_sprintf("%s/dynld", bindir))) return 1;
    File_Paths dirs = { 0 };
    File_Paths c_sources = { 0 };

    if(!walk_directory(&dirs, &c_sources, "src")) return 1;
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    assert(dirs.count == 0);
    File_Paths objs = { 0 };
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* obj = temp_sprintf("%s/dynld%s.o", bindir, strchr(c_sources.items[i], '/'));
        cmd_append(&objs, obj);
        if(nob_c_needs_rebuild1(&stb, &pathb, obj, src)) {
            cmd_append(&cmd, cc,
                "-Wall", "-Wextra", "-Wno-unused-function",
                "-ffunction-sections", "-fdata-sections", "-mgeneral-regs-only",
                "-pie", "-fpie",
                "-nostdlib", "-nolibc", "-ffreestanding",
                "-static", "-g", "-MD",
                "-c",
                src, "-o", obj
            );
            if(!cmd_run_sync_and_reset(&cmd)) return 1;
        }
    }
    const char* exe = temp_sprintf("%s/dynld/dynld", bindir);
    if(needs_rebuild(exe, objs.items, objs.count)) {
        cmd_append(&cmd, cc, "-pie", "-fpie", "-nostdlib", "-nolibc", "-ffreestanding", "-static", "-g");
        da_append_many(&cmd, objs.items, objs.count);
        cmd_append(&cmd, "-o", exe);
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    char* rootdir = getenv("ROOTDIR");
    if(rootdir && !copy_file(exe, temp_sprintf("%s/user/dynld", rootdir))) 
        return 1;
#ifdef COPY_DIR
    if(nob_file_exists(COPY_DIR) == 1 && nob_needs_rebuild1(COPY_DIR EXE, BUILD_DIR EXE) && !nob_copy_file(BUILD_DIR EXE, COPY_DIR EXE)) return 1;
#endif
    return 0;
}


