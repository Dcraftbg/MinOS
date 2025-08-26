#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../nob.h"


// Environment parameters
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
    cmd_append(cmd, "ar", "-cr", archive);
    da_append_many(cmd, inputs, inputs_count);
    return cmd_run_sync_and_reset(cmd);
}
bool dynlink(Nob_Cmd* cmd, const char* so, const char **inputs, size_t inputs_count) {
    cmd_append(cmd, "gcc", "-shared", "-o", so, "-Wl,--hash-style=sysv", "-nolibc");
    da_append_many(cmd, inputs, inputs_count);
    return cmd_run_sync_and_reset(cmd);
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
        return_defer(false);
    }

    errno = 0;
    struct dirent *ent = readdir(dir);
    while (ent != NULL) {
        const char* ent_d_name = temp_strdup(ent->d_name);
        const char* fext = get_ext(ent_d_name);
        const char* path = temp_sprintf("%s/%s",dirpath,ent_d_name);
        Nob_File_Type type = get_file_type(path);
        
        if(fext && strcmp(fext, "o") == 0) {
            if(type == NOB_FILE_REGULAR) {
                da_append(paths,path);
            }
        }
        if (type == NOB_FILE_DIRECTORY) {
            if(strcmp(ent_d_name, ".") != 0 && strcmp(ent_d_name, "..") != 0) {
                sb.count = 0;
                sb_append_cstr(&sb,temp_sprintf("%s/%s",dirpath,ent_d_name));
                sb_append_null(&sb);
                if(!find_objs(sb.items, paths)) return_defer(false);
            }
        }
        ent = readdir(dir);
    }

    if (errno != 0) {
        nob_log(NOB_ERROR, "Could not read directory %s: %s", dirpath, strerror(errno));
        return_defer(false);
    }

defer:
    if (dir) closedir(dir);
    sb_free(sb);
    return result;
}
bool copy_if_needed(const char* from, const char* to) {
    return needs_rebuild1(to, from) ? copy_file(from, to) : true;
}
bool setup_sysroot(Nob_Cmd* cmd) {
    char* minos_root = getenv("MINOSROOT");
    if(!minos_root) {
        nob_log(NOB_ERROR, "Missing the MINOSROOT environmental variable!");
        nob_log(NOB_INFO, "MINOSROOT should contain an absolute path to MinOS's root directory");
        return false;
    }
    // I know KROOT exists, but we already depend on the MINOSROOT
    // to exist so I feel like its just easier for me to depend on less and do more
    char* kroot = temp_sprintf("%s/kernel", minos_root);
    if(!nob_mkdir_if_not_exists_silent(SYSROOT "/usr")) return false;
    if(!nob_mkdir_if_not_exists_silent(SYSROOT "/usr/lib")) return false;
    if(!nob_copy_need_update_directory_recursively(temp_sprintf("%s/user/libc/include", minos_root), SYSROOT "/usr/include")) return false;
    if(!nob_copy_need_update_directory_recursively(temp_sprintf("%s/shared/include", kroot), SYSROOT "/usr/include")) return false;


    const char* crt[] = {
        "crt0.o", "crtn.o", "crtbegin.o", "crti.o", "crtend.o"
    };
    for(size_t i = 0; i < ARRAY_LEN(crt); ++i) {
        if(!copy_if_needed(temp_sprintf("%s/bin/crt/%s", minos_root, crt[i]), temp_sprintf(SYSROOT "/usr/lib/%s", crt[i]))) return false;
    }
    Nob_File_Paths paths = { 0 };
    if(!find_objs(temp_sprintf("%s/bin/libc", minos_root), &paths) || !find_objs(temp_sprintf("%s/bin/shared", minos_root), &paths)) {
        da_free(paths);
        return false;
    }
    if(!ar(cmd, SYSROOT "/usr/lib/libc.a", paths.items, paths.count)) {
        da_free(paths);
        return false;
    }
    if(!dynlink(cmd, SYSROOT "/usr/lib/libc.so", paths.items, paths.count)) {
        da_free(paths);
        return false;
    }
    if(mkdir_if_not_exists(temp_sprintf("%s/initrd/lib", minos_root)) && !copy_file(SYSROOT "/usr/lib/libc.so", temp_sprintf("%s/initrd/lib/libc.so", minos_root))) {
        da_free(paths);
        return false;
    } 

    da_free(paths);
    return true;
}
bool enter_clean_build(void) {
    if(!mkdir_if_not_exists("build")) return false;
    return set_current_dir("build");
}
// TODO: fix temp leakage on error + go back to original directory
// Doesn't really matter but its good to cleanup
bool build_binutils(Nob_Cmd* cmd) {
    size_t temp = temp_save();
    const char *curdir = get_current_dir_temp();
    const char *binutils_dir_abs = nob_temp_realpath(BINUTILS_DIR);
    const char *sysroot_abs = nob_temp_realpath(SYSROOT);
    assert(set_current_dir(BINUTILS_DIR));
    switch(file_exists(BINUTILS_VERSION)) {
    case -1: return false;
    case 0:
        if(file_exists(BINUTILS_TAR) <= 0) {
            cmd_append(cmd, "wget", BINUTILS_URL);
            if(!cmd_run_sync_and_reset(cmd)) return false;
        }
        cmd_append(cmd, "tar", "-xf", BINUTILS_TAR);
        if(!cmd_run_sync_and_reset(cmd)) return false;
        nob_log(NOB_INFO, "current directory: %s", get_current_dir_temp());
        cmd_append(cmd, "patch", "-ruN", "-p1", "-d", BINUTILS_VERSION, "-i", "../../../binutils-2.39.patch");
        if(!cmd_run_sync_and_reset(cmd)) return false;
        cmd_append(cmd, "patch", "-ruN", "-p1", "-d", BINUTILS_VERSION, "-i", "../../../0001-binutils-Add-dynamic-link-support.patch");
        if(!cmd_run_sync_and_reset(cmd)) return false;
        break;
    case 1: break;
    default:
        NOB_UNREACHABLE("file_exists return");
    }
    assert(set_current_dir(BINUTILS_VERSION));
    if(!enter_clean_build()) return false;
    cmd_append(
        cmd, "../configure",
        "--target=x86_64-minos",
        // "--disable-shared",
        temp_sprintf("--prefix=%s", binutils_dir_abs),
        temp_sprintf("--with-sysroot=%s", sysroot_abs),
        "--enable-initfini-array",
        "--enable-lto",
        "--disable-nls",
        "--disable-werror");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    cmd_append(cmd, "make", temp_sprintf("-j%zu", (size_t)NUM_THREADS));
    if(!cmd_run_sync_and_reset(cmd)) return false;
    cmd_append(cmd, "make", "install");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    temp_rewind(temp);
    assert(set_current_dir(curdir));
    return true;
}
bool build_gcc(Nob_Cmd* cmd) {
    size_t temp = temp_save();
    const char *curdir = get_current_dir_temp();
    const char *binutils_dir_abs = nob_temp_realpath(BINUTILS_DIR);
    const char *sysroot_abs = nob_temp_realpath(SYSROOT);
    assert(set_current_dir(GCC_DIR));
    switch(file_exists(GCC_VERSION)) {
    case -1: return false;
    case 0:
        if(file_exists(GCC_TAR) <= 0) {
            cmd_append(cmd, "wget", GCC_URL);
            if(!cmd_run_sync_and_reset(cmd)) return false;
        }
        cmd_append(cmd, "tar", "-xf", GCC_TAR);
        if(!cmd_run_sync_and_reset(cmd)) return false;
        cmd_append(cmd, "patch", "-ruN", "-p1", "-d", GCC_VERSION, "-i", "../../../gcc-12.2.0.patch");
        if(!cmd_run_sync_and_reset(cmd)) return false;
        cmd_append(cmd, "patch", "-ruN", "-p1", "-d", GCC_VERSION, "-i", "../../../0001-minos-Add-config-for-dynamic-linking.patch");
        if(!cmd_run_sync_and_reset(cmd)) return false;
        break;
    case 1: break;
    default:
        NOB_UNREACHABLE("file_exists return");
    }
    assert(set_current_dir(GCC_VERSION));
    if(!enter_clean_build()) return false;
    cmd_append(
        cmd, "../configure",
        "--target=x86_64-minos",
        // "--disable-shared",
        temp_sprintf("--prefix=%s", binutils_dir_abs),
        temp_sprintf("--with-sysroot=%s", sysroot_abs),
        "--enable-initfini-array",
        "--enable-lto",
        "--disable-nls",
        "--enable-languages=c");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    cmd_append(cmd, "make", temp_sprintf("-j%zu", (size_t)NUM_THREADS), "all-gcc");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    cmd_append(cmd, "make", temp_sprintf("-j%zu", (size_t)NUM_THREADS), "all-target-libgcc", "CFLAGS_FOR_TARGET=-mcmodel=large -mno-red-zone");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    cmd_append(cmd, "make", "install-gcc");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    cmd_append(cmd, "make", "install-target-libgcc");
    if(!cmd_run_sync_and_reset(cmd)) return false;
    assert(set_current_dir(curdir));
    temp_rewind(temp);
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
    const char* exe = shift_args(&argc, &argv);
    if(!mkdir_if_not_exists("bin/")) return 1;
    if(!mkdir_if_not_exists(BINUTILS_DIR)) return 1;
    if(!mkdir_if_not_exists(GCC_DIR)) return 1;
    if(!mkdir_if_not_exists(SYSROOT)) return 1;

    if(argc <= 0) {
        if(!setup_sysroot(&cmd)) return 1;
        if(!build_binutils(&cmd)) return 1;
        if(!build_gcc(&cmd)) return 1;
    }

    while(argc > 0) {
        const char *subcmd = shift_args(&argc, &argv);
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


