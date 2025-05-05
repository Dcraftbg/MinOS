#define NOB_IMPLEMENTATION
#include "../nob.h"


// Environment parameters
#ifndef MINOSROOT
#   define MINOSROOT "../../"
#endif
// Number of threads (jobs) to pass to make in -j
// by default its 12 since gcc builds slow as shit
#ifndef NUM_THREADS
#   define NUM_THREADS 12
#endif


// Versions + defines:
#define GCC_VERSION "gcc-12.2.0"
#define GCC_TAR     GCC_VERSION".tar.gz"
#define GCC_URL     "https://ftp.gnu.org/gnu/gcc/"GCC_VERSION"/"GCC_TAR
#define GCC_DIR     "bin/gcc"

#define BINUTILS_VERSION "binutils-2.39"
#define BINUTILS_TAR     BINUTILS_VERSION".tar.gz"
#define BINUTILS_URL     "https://ftp.gnu.org/gnu/binutils/"BINUTILS_TAR
#define BINUTILS_DIR     "bin/binutils"

#define SYSROOT "bin/sysroot"
// TODO: dedicated installation folder.

bool ar(Nob_Cmd* cmd, const char* archive, const char **inputs, size_t inputs_count) {
    nob_cmd_append(cmd, "ar", "-cr", archive);
    nob_da_append_many(cmd, inputs, inputs_count);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool dynlink(Nob_Cmd* cmd, const char* so, const char **inputs, size_t inputs_count) {
    nob_cmd_append(cmd, "gcc", "-shared", "-o", so, "-Wl,--hash-style=sysv", "-nolibc");
    nob_da_append_many(cmd, inputs, inputs_count);
    return nob_cmd_run_sync_and_reset(cmd);
}

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
// TODO: make this function better
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

bool setup_sysroot(Nob_Cmd* cmd) {
    if(!nob_mkdir_if_not_exists_silent(SYSROOT "/usr")) return false;
    if(!nob_mkdir_if_not_exists_silent(SYSROOT "/usr/lib")) return false;
    if(!nob_copy_need_update_directory_recursively(MINOSROOT "user/libc/include", SYSROOT "/usr/include")) return false;
    if(!nob_copy_need_update_directory_recursively(MINOSROOT "libs/std/include", SYSROOT "/usr/include")) return false;
    if(nob_needs_rebuild1(SYSROOT "/usr/lib/crt0.o", MINOSROOT "bin/user/crt/start.o") && !nob_copy_file(MINOSROOT "bin/user/crt/start.o", SYSROOT "/usr/lib/crt0.o")) return false;
    Nob_File_Paths paths = { 0 };
    if(!find_objs(MINOSROOT "bin/user/libc", &paths) || !find_objs(MINOSROOT "bin/std/", &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!ar(cmd, SYSROOT "/usr/lib/libc.a", paths.items, paths.count)) {
        nob_da_free(paths);
        return false;
    }
    if(!dynlink(cmd, SYSROOT "/usr/lib/libc.so", paths.items, paths.count)) {
        nob_da_free(paths);
        return false;
    }
    if(nob_mkdir_if_not_exists(MINOSROOT "initrd/lib") && !nob_copy_file(SYSROOT "/usr/lib/libc.so", MINOSROOT "initrd/lib/libc.so")) {
        nob_da_free(paths);
        return false;
    } 

    nob_da_free(paths);
    return true;
}
bool enter_clean_build(void) {
    if(!nob_mkdir_if_not_exists("build")) return false;
    return nob_set_current_dir("build");
}
// TODO: fix temp leakage on error + go back to original directory
// Doesn't really matter but its good to cleanup
bool build_binutils(Nob_Cmd* cmd) {
    size_t temp = nob_temp_save();
    const char *curdir = nob_get_current_dir_temp();
    const char *binutils_dir_abs = nob_temp_realpath(BINUTILS_DIR);
    const char *sysroot_abs = nob_temp_realpath(SYSROOT);
    assert(nob_set_current_dir(BINUTILS_DIR));
    switch(nob_file_exists(BINUTILS_VERSION)) {
    case -1: return false;
    case 0:
        if(nob_file_exists(BINUTILS_TAR) <= 0) {
            nob_cmd_append(cmd, "wget", BINUTILS_URL);
            if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        }
        nob_cmd_append(cmd, "tar", "-xf", BINUTILS_TAR);
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        nob_log(NOB_INFO, "current directory: %s", nob_get_current_dir_temp());
        nob_cmd_append(cmd, "patch", "-ruN", "-p1", "-d", BINUTILS_VERSION, "-i", "../../../binutils-2.39.patch");
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        nob_cmd_append(cmd, "patch", "-ruN", "-p1", "-d", BINUTILS_VERSION, "-i", "../../../0001-binutils-Add-dynamic-link-support.patch");
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        break;
    case 1: break;
    default:
        NOB_UNREACHABLE("nob_file_exists return");
    }
    assert(nob_set_current_dir(BINUTILS_VERSION));
    if(!enter_clean_build()) return false;
    nob_cmd_append(
        cmd, "../configure",
        "--target=x86_64-minos",
        // "--disable-shared",
        nob_temp_sprintf("--prefix=%s", binutils_dir_abs),
        nob_temp_sprintf("--with-sysroot=%s", sysroot_abs),
        "--enable-initfini-array",
        "--enable-lto",
        "--disable-nls",
        "--disable-werror");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_cmd_append(cmd, "make", nob_temp_sprintf("-j%zu", (size_t)NUM_THREADS));
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_cmd_append(cmd, "make", "install");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_temp_rewind(temp);
    assert(nob_set_current_dir(curdir));
    return true;
}
bool build_gcc(Nob_Cmd* cmd) {
    size_t temp = nob_temp_save();
    const char *curdir = nob_get_current_dir_temp();
    const char *binutils_dir_abs = nob_temp_realpath(BINUTILS_DIR);
    const char *sysroot_abs = nob_temp_realpath(SYSROOT);
    assert(nob_set_current_dir(GCC_DIR));
    switch(nob_file_exists(GCC_VERSION)) {
    case -1: return false;
    case 0:
        if(nob_file_exists(GCC_TAR) <= 0) {
            nob_cmd_append(cmd, "wget", GCC_URL);
            if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        }
        nob_cmd_append(cmd, "tar", "-xf", GCC_TAR);
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        nob_cmd_append(cmd, "patch", "-ruN", "-p1", "-d", GCC_VERSION, "-i", "../../../gcc-12.2.0.patch");
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        nob_cmd_append(cmd, "patch", "-ruN", "-p1", "-d", GCC_VERSION, "-i", "../../../0001-minos-Add-config-for-dynamic-linking.patch");
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
        break;
    case 1: break;
    default:
        NOB_UNREACHABLE("nob_file_exists return");
    }
    assert(nob_set_current_dir(GCC_VERSION));
    if(!enter_clean_build()) return false;
    nob_cmd_append(
        cmd, "../configure",
        "--target=x86_64-minos",
        // "--disable-shared",
        nob_temp_sprintf("--prefix=%s", binutils_dir_abs),
        nob_temp_sprintf("--with-sysroot=%s", sysroot_abs),
        "--enable-initfini-array",
        "--enable-lto",
        "--disable-nls",
        "--enable-languages=c");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_cmd_append(cmd, "make", nob_temp_sprintf("-j%zu", (size_t)NUM_THREADS), "all-gcc");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_cmd_append(cmd, "make", nob_temp_sprintf("-j%zu", (size_t)NUM_THREADS), "all-target-libgcc", "CFLAGS_FOR_TARGET=-mcmodel=large -mno-red-zone");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_cmd_append(cmd, "make", "install-gcc");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    nob_cmd_append(cmd, "make", "install-target-libgcc");
    if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    assert(nob_set_current_dir(curdir));
    nob_temp_rewind(temp);
    return true;
}
void help(FILE* sink, const char* exe) {
    fprintf(sink, "%s (subcmd...)\n", exe);
    fprintf(sink, "Subcmds:\n");
    fprintf(sink, "  - sysroot  - updates sysroot\n");
    fprintf(sink, "  - binutils - builds binutils\n");
    fprintf(sink, "  - gcc      - builds gcc\n");
    fprintf(sink, "  - all      - Builds everything (called by default)\n");
}
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    const char* exe = nob_shift_args(&argc, &argv);
    if(!nob_mkdir_if_not_exists("bin/")) return 1;
    if(!nob_mkdir_if_not_exists(BINUTILS_DIR)) return 1;
    if(!nob_mkdir_if_not_exists(GCC_DIR)) return 1;
    if(!nob_mkdir_if_not_exists(SYSROOT)) return 1;

    if(argc <= 0) {
        if(!setup_sysroot(&cmd)) return 1;
        if(!build_binutils(&cmd)) return 1;
        if(!build_gcc(&cmd)) return 1;
    }

    while(argc > 0) {
        const char *subcmd = nob_shift_args(&argc, &argv);
        if(strcmp(subcmd, "sysroot") == 0) {
            if(!setup_sysroot(&cmd)) return 1;
        }
        else if(strcmp(subcmd, "binutils") == 0) {
            if(!build_binutils(&cmd)) return 1;
        }
        else if(strcmp(subcmd, "gcc") == 0) {
            if(!build_gcc(&cmd)) return 1;
        }
        else if(strcmp(subcmd, "all") == 0) {
            if(!setup_sysroot(&cmd)) return 1;
            if(!build_binutils(&cmd)) return 1;
            if(!build_gcc(&cmd)) return 1;
        }
        else {
            fprintf(stderr, "ERROR: Unknown subcommand: `%s`\n", subcmd);
            help(stderr, exe);
            return 1;
        }
    }
    return 0;
}


