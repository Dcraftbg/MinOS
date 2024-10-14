#pragma once
bool cc(const char* ipath, const char* opath);
bool cc_user(const char* ipath, const char* opath);
bool nasm(const char* ipath, const char* opath);
typedef struct {
    // Build .c -> .o
    bool (*cc  )(const char* ipath, const char* opath);
    bool (*nasm)(const char* ipath, const char* opath);
} BuildFuncs;

extern BuildFuncs kernel_funcs;
extern BuildFuncs user_funcs;
bool _build_dir(BuildFuncs* funcs, const char* rootdir, const char* build_dir, const char* srcdir, bool forced);
static bool build_kernel_dir(const char* rootdir, const char* build_dir, bool forced) {
    return _build_dir(&kernel_funcs, rootdir, build_dir, rootdir, forced);
}
static bool build_user_dir(const char* rootdir, const char* build_dir, bool forced) {
    return _build_dir(&user_funcs, rootdir, build_dir, rootdir, forced); 
}
