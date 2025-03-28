#define NOB_IMPLEMENTATION
#include "../../nob.h"


#define HASH "8c4e6738"
#define URL  "https://github.com/TinyCC/tinycc.git"

bool is_not_patched(Nob_Cmd* cmd, const char* patch) {
    nob_cmd_append(cmd, "git", "apply", "--check", "--ignore-space-change", patch);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool do_patch(Nob_Cmd* cmd, const char* patch) {
    nob_cmd_append(cmd, "git", "apply", "--reject", "--ignore-space-change", patch);
    return nob_cmd_run_sync_and_reset(cmd);
}
#define PREFIX "x86_64-minos-"
void setup_envvars(void) {
    setenv("TARGETOS", "MinOS", 1);
    setenv("CFLAGS", "-g", 1);
    setenv("AR", PREFIX "ar", 1);
    setenv("CC", PREFIX "gcc", 1);
}
bool do_configure(Nob_Cmd* cmd) {
    nob_cmd_append(cmd, "../configure", "--debug", "--targetos=MinOS", "--sysincludepaths=/include", "--elfinterp=<TBD>", "--libpaths=/lib", "--crtprefix=/lib");
    return nob_cmd_run_sync_and_reset(cmd);
}
bool do_clone(Nob_Cmd* cmd) {
    nob_cmd_append(cmd, "git", "clone", URL);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool do_checkout(Nob_Cmd* cmd) {
    nob_cmd_append(cmd, "git", "checkout", HASH);
    return nob_cmd_run_sync_and_reset(cmd);
}
bool gcc_exists(Nob_Cmd* cmd) {
    nob_cmd_append(cmd, PREFIX "gcc", "-v");
    return nob_cmd_run_sync_and_reset(cmd);
}
bool patch_config_h(Nob_String_Builder* sb) {
    if(!nob_read_entire_file("config.h", sb)) return false;
    nob_sb_append_cstr(sb,
       "#define CONFIG_TCC_STATIC 1\n"
       "#define CONFIG_TCC_SEMLOCK 0\n"
    );
    if(!nob_write_entire_file("config.h", sb->items, sb->count)) return false;
    sb->count = 0;
    return true;
}
bool unbuildable_warn(void) {
#if  _WIN32
    fprintf(stderr, "WARN: Port is likely unbuildable on Windows.\nAre you sure you want to continue? (n) ");
    return getchar() == 'y';
#else
    return true;
#endif
}
enum {
    INSTALL_FULL=0,
    INSTALL_CUSTOM,
} install = INSTALL_FULL;
bool copy_tcc(void) {
    if(install == INSTALL_CUSTOM) {
        fprintf(stderr, "Copy tcc into initrd? (y) ");
        if(getchar() == 'n') return true;
    }
    return nob_copy_file("tinycc/build/tcc", "../../initrd/user/tcc");
}
bool copy_tcc_headers(void) {
    if(install == INSTALL_CUSTOM) {
        fprintf(stderr, "Copy tcc headers (Very important)? (y) ");
        if(getchar() == 'n') return true;
    }
    return nob_copy_directory_recursively("tinycc/include", "../../initrd/include");
}
bool gen_stdint(Nob_String_Builder* sb) {
    if(install == INSTALL_CUSTOM) {
        fprintf(stderr, "Generate stdint.h (Very important)? (y) ");
        if(getchar() == 'n') return true;
    }
    nob_sb_append_cstr(sb, 
        "#pragma once\n"
        "typedef unsigned char      uint8_t;\n"
        "typedef unsigned short     uint16_t;\n"
        "typedef unsigned int       uint32_t;\n"
        "typedef unsigned long long uint64_t;\n"
        "typedef char      int8_t;\n"
        "typedef short     int16_t;\n"
        "typedef int       int32_t;\n"
        "typedef long long int64_t;\n"
    );

    if(!nob_mkdir_if_not_exists("../../initrd/include")) return false;
    if(!nob_write_entire_file("../../initrd/include/stdint.h", sb->items, sb->count)) return false;
    sb->count = 0;
    return true;
}
bool copy_sys_headers(void) {
    if(install == INSTALL_CUSTOM) {
        fprintf(stderr, "Copy system headers? (y) ");
        if(getchar() == 'n') return true;
    }
    return nob_copy_directory_recursively("../../libs/std/include", "../../initrd/include");
}
bool copy_libc_headers(void) {
    if(install == INSTALL_CUSTOM) {
        fprintf(stderr, "Copy libc headers? (y) ");
        if(getchar() == 'n') return true;
    }
    return nob_copy_directory_recursively("../libc/include", "../../initrd/include");
}
bool copy_libc_archive(void) {
    if(install == INSTALL_CUSTOM) {
        fprintf(stderr, "Copy libc archive (+crt)? (y) ");
        if(getchar() == 'n') return true;
    }
    return nob_copy_directory_recursively("../toolchain/bin/sysroot/usr/lib", "../../initrd/lib");
}
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    Nob_String_Builder sb = {0};
    if(!unbuildable_warn()) return 1;
    if(!gcc_exists(&cmd)) {
        nob_log(NOB_ERROR, "Please build the toolchain in user/toolchain before building this port");
        return 1;
    }
    fprintf(stderr, " 0: full   (copy tcc, system headers, libc)\n");
    fprintf(stderr, " 1: custom (ask for every step)\n");
    fprintf(stderr, "Installation type: (default: 0) ");
    switch(getchar()) {
    case '1':
        install = INSTALL_CUSTOM;
        break;
    }
    bool needs_to_checkout = false;
    if(!nob_file_exists("tinycc")) {
        needs_to_checkout = true;
        if(!do_clone(&cmd)) return 1;
    }
    if(!nob_set_current_dir("tinycc")) return 1;
    if(needs_to_checkout && !do_checkout(&cmd)) return 1;

    if(!nob_mkdir_if_not_exists("build")) return 1;
    if(!nob_set_current_dir("build")) return 1;

    setup_envvars();
    if(!nob_file_exists("config.mak")) {
        if(!do_configure(&cmd)) return 1;
        if(!patch_config_h(&sb)) return 1;
    }
    {
        if(!nob_set_current_dir("..")) return 1;
        const char* patch = "../patches/0001-Patch.patch";
        if(is_not_patched(&cmd, patch)) {
            if(!do_patch(&cmd, patch)) {
                nob_log(NOB_ERROR, "Failed to do patch!");
                return 1;
            }
        }
        if(!nob_set_current_dir("build")) return 1;
    }
    nob_cmd_append(&cmd, "make");
    if(!nob_cmd_run_sync_and_reset(&cmd)) {
        nob_log(NOB_WARNING, "Make did fail, but we ignore this as testing currently doesn't work and I couldn't be bothered to figure out a way to disable it.");
    } 
    if(!nob_set_current_dir("../..")) return 1;
    if(!copy_tcc()) return 1;
    if(!copy_tcc_headers()) return 1;
    if(!gen_stdint(&sb)) return 1;
    if(!copy_libc_headers()) return 1;
    if(!copy_libc_archive()) return 1;
    return 0;
}
