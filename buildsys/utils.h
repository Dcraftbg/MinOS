#pragma once
const char* get_ext(const char* path);
const char* get_base(const char* path);
char* shift_args(int *argc, char ***argv);
const char* strip_prefix(const char* str, const char* prefix);
bool nob_mkdir_if_not_exists_silent(const char *path);
bool remove_objs(const char* dirpath);
