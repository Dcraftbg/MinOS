#include "flags.h"
void cleanup_d(const char* obj) {
    size_t temp = nob_temp_save();
    char* str = nob_temp_strdup(obj);
    size_t str_len = strlen(str);
    assert(str_len);
    str[str_len-1] = 'd';
    nob_delete_file(str);
    nob_temp_rewind(temp);
}
// TODO: cc but async
bool cc(const char* ipath, const char* opath) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, GCC);
    nob_cmd_append(&cmd, CFLAGS);
    nob_cmd_append(&cmd, "-I", "./kernel/vendor/limine");
    nob_cmd_append(&cmd, "-I", "./kernel/vendor/stb");
    nob_cmd_append(&cmd, "-c", ipath, "-o", opath);
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       cleanup_d(opath);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}

bool ld(Nob_File_Paths* paths, const char* opath, const char* ldscript) {
    if(!nob_needs_rebuild1(opath, ldscript) && !nob_needs_rebuild(opath, paths->items, paths->count))
        return true;
    nob_log(NOB_INFO, "Linking %s",opath);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, LD);
#ifdef LDFLAGS
    nob_cmd_append(&cmd, LDFLAGS);
#endif
    if(ldscript) {
        nob_cmd_append(&cmd, "-T", ldscript);
    }
    nob_cmd_append(&cmd, "-o", opath);
    for(size_t i = 0; i < paths->count; ++i) {
        nob_cmd_append(&cmd, paths->items[i]);
    }
    if(!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    nob_log(NOB_INFO, "Linked %s successfully", opath);
    return true;
}
// TODO: cc but async
bool cc_user(const char* ipath, const char* opath) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, GCC);
    nob_cmd_append(&cmd, USER_CFLAGS);
    nob_cmd_append(&cmd, "-I", "./kernel/vendor/stb");
    nob_cmd_append(&cmd, "-I", "./user/libc/include");
    nob_cmd_append(&cmd, "-c", ipath, "-o", opath);
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       cleanup_d(opath);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}
// TODO: nasm but async
bool nasm(const char* ipath, const char* opath) {
    Nob_Cmd cmd = {0};
    const char* end = get_base(ipath);
    nob_cmd_append(&cmd, "nasm");
    if(end) {
        Nob_String_View sv = {0};
        sv.data = ipath;
        sv.count = end-ipath;
        nob_cmd_append(&cmd, "-I", nob_temp_sv_to_cstr(sv));
    }
    nob_cmd_append(&cmd, "-f", "elf64", ipath, "-o", opath);
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}

BuildFuncs kernel_funcs = {
    .cc=cc,
    .nasm=nasm
};
BuildFuncs user_funcs = {
    .cc=cc_user,
    .nasm=nasm,
};

bool _build_dir(BuildFuncs* funcs, const char* rootdir, const char* build_dir, const char* srcdir, bool forced) {
    bool result = true;
    Nob_String_Builder opath = {0};
    DIR *dir = opendir(srcdir);
    if (dir == NULL) {
        nob_log(NOB_ERROR, "Could not open directory %s: %s",srcdir,strerror(errno));
        return false;
    }
    errno = 0;
    struct dirent *ent = readdir(dir);
    while(ent != NULL) {
         const char* fext = get_ext(ent->d_name);
         const char* path = nob_temp_sprintf("%s/%s",srcdir,ent->d_name); 
         Nob_File_Type type = nob_get_file_type(path);

         if(type == NOB_FILE_REGULAR) {
            if(strcmp(fext, "c")==0) {
                opath.count = 0;
                nob_sb_append_cstr(&opath, build_dir);
                nob_sb_append_cstr(&opath, "/");
                const char* file = strip_prefix(path, rootdir)+1;
                Nob_String_View sv = nob_sv_from_cstr(file);
                sv.count-=2; // Remove .c
                nob_sb_append_buf(&opath,sv.data,sv.count);
                nob_sb_append_cstr(&opath, ".d");
                nob_sb_append_null(&opath);
                if((!nob_file_exists(opath.items)) || nob_needs_rebuild1(opath.items, path) || forced) {
                    opath.items[opath.count-2] = 'o';
                    if(!funcs->cc(path, opath.items)) nob_return_defer(false);
                } else {
                    Nob_String_Builder dep_sb={0};
                    if(!nob_read_entire_file(opath.items, &dep_sb)) {
                        nob_return_defer(false);
                    }
                    nob_sb_append_null(&dep_sb);
                    Nob_File_Paths dep_paths={0};
                    char* obj=NULL;
                    if(!dep_analyse_str(dep_sb.items, &obj, &dep_paths)) {
                        nob_sb_free(dep_sb);
                        nob_da_free(dep_paths);
                        nob_return_defer(false);
                    }
                    if(nob_needs_rebuild(opath.items, dep_paths.items, dep_paths.count)) {
                        if(!funcs->cc(path, obj)) {
                            nob_sb_free(dep_sb);
                            nob_da_free(dep_paths);
                            nob_return_defer(false);
                        }
                    }
                    nob_sb_free(dep_sb);
                    nob_da_free(dep_paths);
                }
            } else if(strcmp(fext, "nasm") == 0) {
                opath.count = 0;
                nob_sb_append_cstr(&opath, build_dir);
                nob_sb_append_cstr(&opath, "/");
                const char* file = strip_prefix(path, rootdir)+1;
                Nob_String_View sv = nob_sv_from_cstr(file);
                sv.count-=5; // Remove .nasm
                nob_sb_append_buf(&opath,sv.data,sv.count);
                nob_sb_append_cstr(&opath, ".o");
                nob_sb_append_null(&opath);
                if((!nob_file_exists(opath.items)) || nob_needs_rebuild1(opath.items,path) || forced) {
                    if(!funcs->nasm(path,opath.items)) nob_return_defer(false);
                }
            }
         }
         if (type == NOB_FILE_DIRECTORY && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
          
            opath.count = 0;
            nob_sb_append_cstr(&opath, build_dir);
            nob_sb_append_cstr(&opath, "/");
            nob_sb_append_cstr(&opath, strip_prefix(path, rootdir)+1);
            nob_sb_append_null(&opath);
            if(!nob_mkdir_if_not_exists_silent(opath.items)) nob_return_defer(false);
            opath.count = 0;
            nob_sb_append_cstr(&opath, path);
            nob_sb_append_null(&opath);
            if(!_build_dir(funcs, rootdir, build_dir, opath.items, forced)) nob_return_defer(false);
         }
         ent = readdir(dir);
    }
defer:
    if (dir) closedir(dir);
    if (opath.items) nob_sb_free(opath);
    return result;
}
