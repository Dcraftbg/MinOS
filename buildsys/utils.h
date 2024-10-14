#pragma once
const char* get_ext(const char* path);
const char* get_base(const char* path);
char* shift_args(int *argc, char ***argv);
const char* strip_prefix(const char* str, const char* prefix);
bool nob_mkdir_if_not_exists_silent(const char *path);
bool remove_objs(const char* dirpath);
void eat_arg(Build* b, size_t arg);
bool _copy_all_to(const char* to, const char** paths, size_t paths_count);
#define copy_all_to(to, ...) \
    _copy_all_to((to), \
                 ((const char*[]){__VA_ARGS__}), \
                 (sizeof((const char*[]){__VA_ARGS__})/sizeof(const char*)))
