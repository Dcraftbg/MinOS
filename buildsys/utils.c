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
