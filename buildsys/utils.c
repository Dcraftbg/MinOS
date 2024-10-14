const char* get_ext(const char* path) {
    const char* end = path;
    while(*end) end++;
    while(end >= path) {
        if(*end == '.') return end+1;
        if(*end == '/' || *end == '\\') break;
        end--;
    }
    return NULL;
}
const char* get_base(const char* path) {
    const char* end = path;
    while(*end) end++;
    while(end >= path) {
        if(*end == '/' || *end == '\\') return end+1;
        end--;
    }
    return end;
}
char* shift_args(int *argc, char ***argv) {
    if((*argc) <= 0) return NULL;
    char* arg = **argv;
    (*argc)--;
    (*argv)++;
    return arg;
}
const char* strip_prefix(const char* str, const char* prefix) {
    size_t len = strlen(prefix);
    if(strncmp(str, prefix, strlen(prefix))==0) return str+len;
    return NULL;
}
bool nob_mkdir_if_not_exists_silent(const char *path) {
    if(nob_file_exists(path)) return true;
    return nob_mkdir_if_not_exists(path);
}
bool remove_objs(const char* dirpath) {
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        nob_log(NOB_ERROR, "Could not open directory %s: %s",dirpath,strerror(errno));
        return false;
    }
    errno = 0;
    struct dirent *ent = readdir(dir);
    while(ent != NULL) {
         const char* fext = get_ext(ent->d_name);
         const char* path = nob_temp_sprintf("%s/%s",dirpath,ent->d_name); 
         Nob_File_Type type = nob_get_file_type(path);
         if(strcmp(fext, "o")==0) {
             if(type == NOB_FILE_REGULAR) {
                if(!nob_delete_file(path)) {
                   closedir(dir);
                   return false;
                }
             }
         }
         if (type == NOB_FILE_DIRECTORY) {
             Nob_String_Builder sb = {0};
             nob_sb_append_cstr(&sb, path);
             nob_sb_append_null(&sb);
             if(!remove_objs(sb.items)) {
                 nob_sb_free(sb);
                 return false;
             }
             nob_sb_free(sb);
         }
         ent = readdir(dir);
    }
    if (dir) closedir(dir);
    return true;
}

void eat_arg(Build* b, size_t arg) {
    assert(b->argc);
    assert(arg < b->argc);
    size_t total = b->argc--;
    memmove(b->argv+arg, b->argv+arg+1, total-arg-1);
}

bool find_objs(const char* dirpath, Nob_File_Paths *paths) {
    Nob_String_Builder sb={0};
    bool result = true;
    DIR *dir = NULL;

    dir = opendir(dirpath);
    if (dir == NULL) {
        nob_log(NOB_ERROR, "Could not open directory %s: %s", dirpath, strerror(errno));
        nob_return_defer(false);
    }

    errno = 0;
    struct dirent *ent = readdir(dir);
    while (ent != NULL) {
        const char* ent_d_name = nob_temp_strdup(ent->d_name);
        const char* fext = get_ext(ent_d_name);
        const char* path = nob_temp_sprintf("%s/%s",dirpath,ent_d_name);
        Nob_File_Type type = nob_get_file_type(path);
        
        if(fext && strcmp(fext, "o") == 0) {
            if(type == NOB_FILE_REGULAR) {
                nob_da_append(paths,path);
            }
        }
        if (type == NOB_FILE_DIRECTORY) {
            if(strcmp(ent_d_name, ".") != 0 && strcmp(ent_d_name, "..") != 0) {
                sb.count = 0;
                nob_sb_append_cstr(&sb,nob_temp_sprintf("%s/%s",dirpath,ent_d_name));
                nob_sb_append_null(&sb);
                if(!find_objs(sb.items, paths)) nob_return_defer(false);
            }
        }
        ent = readdir(dir);
    }

    if (errno != 0) {
        nob_log(NOB_ERROR, "Could not read directory %s: %s", dirpath, strerror(errno));
        nob_return_defer(false);
    }

defer:
    if (dir) closedir(dir);
    nob_sb_free(sb);
    return result;
}

bool _copy_all_to(const char* to, const char** paths, size_t paths_count) {
    for(size_t i = 0; i < paths_count; ++i) {
        const char* path = nob_temp_sprintf("%s/%s",to,get_base(paths[i]));
        if(!nob_file_exists(path) || nob_needs_rebuild1(path,paths[i])) {
            if(!nob_copy_file(paths[i],path)) return false;
        }
    }
    return true;
}
