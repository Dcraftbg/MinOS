#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../nob.h"

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
        if(strcmp(ent->d_name, "..") == 0 || strcmp(ent->d_name, ".") == 0) continue;
        const char* fext = nob_get_ext(ent->d_name);
        size_t temp = nob_temp_save();
        const char* p = nob_temp_sprintf("%s/%s", path, ent->d_name); 
        Nob_File_Type type = nob_get_file_type(p);
        if(type == NOB_FILE_DIRECTORY) {
            da_append(dirs, p);
            if(!walk_directory(dirs, c_sources, p)) {
                closedir(dir);
                return false;
            }
            continue;
        }
        if(strcmp(fext, "c") == 0) {
            nob_da_append(c_sources, p);
            continue;
        }
        nob_temp_rewind(temp);
    }
    closedir(dir);
    return true;
}
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    char* cc = getenv("CC");
    if(!cc) cc = "cc";
    char* ar = getenv("AR");
    if(!ar) ar = "ar";
    char* bindir = getenv("BINDIR");
    if(!bindir) bindir = "bin";
    if(!nob_mkdir_if_not_exists_silent(bindir)) return 1;
    if(!nob_mkdir_if_not_exists_silent(temp_sprintf("%s/libwm", bindir))) return 1;

    File_Paths dirs = { 0 };
    File_Paths c_sources = { 0 };
    size_t src_dir = strlen("src/");
    if(!walk_directory(&dirs, &c_sources, "src")) return 1;
    for(size_t i = 0; i < dirs.count; ++i) {
        size_t temp = temp_save();
        const char* dir = temp_sprintf("%s/libwm/%s", bindir, dirs.items[i] + src_dir);
        if(!nob_mkdir_if_not_exists_silent(dir)) return 1;
        temp_rewind(temp);
    }
    File_Paths objs = { 0 };
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = nob_temp_sprintf("%s/libwm/%.*s.o", bindir, (int)(strlen(src + 4)-2), src + 4);
        da_append(&objs, out);
        if(!nob_c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        cmd_append(&cmd, cc, "-Wall", "-Wextra", "-MD", "-fPIC", "-I", "include", "-O2", src, "-c", "-o", out);
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
    const char* so = nob_temp_sprintf("%s/libwm/libwm.so", bindir);
    if(needs_rebuild(so, objs.items, objs.count)) {
        cmd_append(&cmd, cc, "-shared", "-o", so, "-Wl,--hash-style=sysv", "-static-libgcc");
        da_append_many(&cmd, objs.items, objs.count);
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }
    const char* archive = nob_temp_sprintf("%s/libwm/libwm.a", bindir);
    if(needs_rebuild(archive, objs.items, objs.count)) {
        cmd_append(&cmd, ar, "-cr", archive);
        da_append_many(&cmd, objs.items, objs.count);
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }

    char* rootdir = getenv("ROOTDIR");
    if(rootdir && !copy_file(so, temp_sprintf("%s/lib/libwm.so", rootdir))) 
        return 1;
}
