// Much love to lordmilko who provided the pre-built binaries for gcc on Win32 and Linux <3 
// Also much love to Tsoding (https://github.com/tsoding) for providing this awesome library for making build 'scripts'! 
//
// Philosophy:
// bootstrap.c is a bootstrapper for the actual build system that is found under
// ./buildsys/
// 
// It relies on two tools being available
// 1. the C compiler used to build the bootstrapper (pretty self explanatory)
// 2. wget command - builtin to linux but NOT windows :( 
// 3. tar command - builtin to linux and windows
//
// MACOS users:
// I don't have pre-compield gcc binaries for MacOS, so you'll need to provide them yourself
// Please pass the -GCC=<path to cross-c. gcc> -LD=<path to cross.c ld>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <winbase.h>
#else
#include <sys/stat.h>
#endif


typedef struct {
    const char* exe;
    const char* gcc;
    const char* ld;
} Build;
char *shift_args(int *argc, char ***argv) {
    if((*argc) <= 0) return NULL;
    char *res = **argv;
    (*argv)++;
    (*argc)--;
    return res;
}
const char* c_compiler = 
#if defined(__GNUC__)
"gcc"
#elif defined(__clang__)
"clang"
#elif defined(__TINYC__)
"tcc"
#else
#error Unknown C compiler :(
#endif
;
bool bootstrap_build() {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, c_compiler, "./buildsys/main.c", "-o");

#if defined(__clang__) && defined(_WIN32)    // Clang is weird on windows as it doesn't add the .exe suffix
    nob_cmd_append(&cmd, "build.exe");
#else 
    nob_cmd_append(&cmd, "build");
#endif
    bool res = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return res;
}
// TODO: Consider using bitsadmin for windows
bool download(const char* url, const char* opath) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "wget");
    if(opath) {
        nob_cmd_append(&cmd, "-O", opath);
    }
    nob_cmd_append(&cmd, url);
    if(!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
#ifdef _WIN32
    #define GCC_ZIP "x86_64-elf-tools-windows.zip"
#else
    #define GCC_ZIP "x86_64-elf-tools-linux.zip"
#endif
#define GCC_DEFAULT "./gcc/bin/x86_64-elf-gcc"
#define LD_DEFAULT  "./gcc/bin/x86_64-elf-ld"
// #define GCC_VERSION "7.1.0"
#define GCC_VERSION "13.2.0"

#if !defined(__MACH__) || defined(__APPLE__)
#define GCC_DOWNLOAD_URL "https://github.com/lordmilko/i686-elf-tools/releases/download/" GCC_VERSION "/" GCC_ZIP, "./gcc/" GCC_ZIP
#endif
bool get_gcc_zip() {
#if defined(__MACH__) || defined(__APPLE__)
    nob_log(NOB_WARNING, "There are not available pre-compiled binaries for MacOS");
    nob_log(NOB_INFO, "If you haven't already, try building x86_64-elf-gcc and x86_64-elf-ld. Instructions for which you can find online (same as i686 if you've built that)");
    nob_log(NOB_INFO, "Please pass -GCC=<path to cross compiler gcc> and -LD=<path to cross compiler ld>");
    nob_log(NOB_INFO, "Sorry for the inconvenience");
    return false;
#else
    return download(GCC_DOWNLOAD_URL);
#endif
}
// TODO: Consider using libzip or something similar
bool unzip(const char* path, const char* dirpath) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "tar", "-xzf", path);
    if(dirpath) {
        nob_cmd_append(&cmd, "-C", dirpath);
    }
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool bootstrap_gcc() {
    if (nob_file_exists("./gcc") && nob_file_exists("./gcc/bin")) {
        // No need to download gcc again
        return true;
    }
    if (!nob_mkdir_if_not_exists("./gcc")) {
        nob_log(NOB_ERROR, "Could not make gcc directory\n");
        return false;
    }
    if(!nob_file_exists("./gcc/" GCC_ZIP)) {
        if (!get_gcc_zip()) {
#ifdef GCC_DOWNLOAD_URL
           nob_log(NOB_INFO, "Could not download gcc. Maybe try downloading it manually:");
           nob_log(NOB_INFO, "URL: %s", GCC_DOWNLOAD_URL);
#endif
           return false;
        }
    }
    if (!unzip("./gcc/" GCC_ZIP, "./gcc")) {
        nob_log(NOB_ERROR, "Could not unzip gcc\n");
        nob_log(NOB_INFO, "Maybe you're missing tar command?");
        nob_log(NOB_INFO, "Try downloading tar command or unzipping gcc yourself");
        return false;
    }
    return true;
}
bool strstarts(const char* str, const char* prefix) {
     return strncmp(str, prefix, strlen(prefix))==0;
}
const char* strip_prefix(const char* str, const char* prefix) {
     size_t len = strlen(prefix);
     if(strncmp(str, prefix, strlen(prefix))==0) return str+len;
     return NULL;
}
bool bootstrap_config(Build* build) {
     nob_log(NOB_INFO, "Generating config.h");
     FILE *f = fopen("config.h", "wb");
     if(!f) {
        nob_log(NOB_ERROR, "Could not generate config.h: %s", strerror(errno)); 
        return false;
     }
     fprintf(f, "//// GCC and LD path to use for building the OS\n");
     const char* gcc = build->gcc ? build->gcc : GCC_DEFAULT; 
     const char* ld  = build->ld  ? build->ld  : LD_DEFAULT;
     fprintf(f, "#define GCC \"%s\"\n", gcc);
     fprintf(f, "#define LD  \"%s\"\n", ld);
     fclose(f);
     return true;
}
bool bootstrap_submodules() {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "git", "submodule", "update", "--init", "--depth", "1");
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc,argv);
    char* arg;
    Build build = {0};
    build.exe = shift_args(&argc, &argv);
    assert(build.exe && "First argument should always be the path to exe");
    const char* gcc=NULL;
    const char* ld=NULL;
    bool forceConfig = false;
    while(arg = shift_args(&argc, &argv)) {
        if (gcc = strip_prefix(arg, "-GCC=")) {
           build.gcc = gcc;
        } else if (ld = strip_prefix(arg, "-LD=")) {
           build.ld = ld;
        } else if (strcmp(arg, "-fconfig")==0) {
           forceConfig = true;
        }
        else {
           nob_log(NOB_ERROR, "Unexpected argument: %s",arg);
           abort();
        }
    }
    if(!bootstrap_submodules()) {
        nob_log(NOB_ERROR, "Failed to bootstrap submodules!");
        abort();
    }
    if((!build.gcc) || (!build.ld)) {
        if(!bootstrap_gcc()) {
            nob_log(NOB_ERROR, "Failed to bootstrap gcc");
            abort();
        }
    }
    if((!nob_file_exists("config.h")) || forceConfig) {
        if(!bootstrap_config(&build)) {
            nob_log(NOB_ERROR, "Failed to bootstrap config.h");
            abort();
        }
    }
    if(!bootstrap_build()) {
        nob_log(NOB_ERROR, "Could not bootstrap the build system");
        abort();
    }
}
