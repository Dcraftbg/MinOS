// TODO: Dependency system, where the buildsys analyses the code and figures out the dependencies of a file and caches them in a .dep file
// .d does NOT work for me rn. I mean I can try to use them but seems a bit of a hassle to parse the Makefile rules

// TODO: church
// A sort of C search project, that will document things inside the project and if ran natively will open an html page
// Served using something like curl. Otherwise if it couldn't open the web page itself, it will open a CLI you can use for searching
//
// The project itself could be ported to MinOS at some point, so you can search documentation from within the OS but thats a bit ahead for our purposes.
// I think for now we must first work on registering devices in user mode, and defining some sort of terminal emulator that will use the PS2 keyboard and a VGA display.
//
// TODO: Usermode drivers
// If you're calling something that is defined in user mode, the kernel should switch out the cr3 and permisions before calling it maybe?
// I'm still not sure how that whole thing will function but I guess we'll see

#include <stdio.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"
#include "../config.h"
#include <stdint.h>
#include "flags.h"
#include "utils.c"
#include "embed.c"
#include "compile.c"

#define LIBC_TARGET_DIR     "./bin/user/libc"
#define LIBC_CRT_TARGET_DIR "./bin/user/crt"
bool make_build_dirs() {
    if(!nob_mkdir_if_not_exists_silent("./bin"             )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/kernel"      )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/std"         )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/iso"         )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user"        )) return false;
    if(!nob_mkdir_if_not_exists_silent(LIBC_TARGET_DIR     )) return false;
    if(!nob_mkdir_if_not_exists_silent(LIBC_CRT_TARGET_DIR )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/nothing")) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/init"   )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/shell"  )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/hello"  )) return false;
    return true;
}
bool clean() {
    if(nob_file_exists("./bin/kernel")) {
        if (!remove_objs("./bin/kernel")) return false;
    }
    return true;
}

bool build_std(bool forced) {
    if(!build_kernel_dir("./libs/std/src", "./bin/std"   , forced)) return false;
    nob_log(NOB_INFO, "Built std successfully");
    return true;
}
bool build_kernel(bool forced) {
    if(!build_kernel_dir("./kernel/src"  , "./bin/kernel", forced)) return false;
    nob_log(NOB_INFO, "Built kernel successfully");
    return true;
}
bool make_iso() {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "xorriso",
        "-as", "mkisofs",
        "-b", "limine-bios-cd.bin",
        "-no-emul-boot",
        "-boot-load-size", "4",
        "-boot-info-table",
        "--efi-boot", "limine-uefi-cd.bin",
        "-efi-boot-part",
        "--efi-boot-image",
        "./bin/iso",
        "-o",
        "./bin/OS.iso"
    );
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
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
bool ld(Nob_File_Paths* paths, const char* opath, const char* ldscript) {
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
bool link_kernel() {
    Nob_File_Paths paths = {0};
    if(!find_objs("./bin/kernel",&paths)) {
        nob_da_free(paths);
        return false;
    }

    if(!find_objs("./bin/std",&paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, "./bin/iso/kernel", "./linker/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    return true;
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
#define copy_all_to(to, ...) \
    _copy_all_to((to), \
                 ((const char*[]){__VA_ARGS__}), \
                 (sizeof((const char*[]){__VA_ARGS__})/sizeof(const char*)))
bool make_limine() {
    if(!copy_all_to(
        "./bin/iso",
        "./kernel/vendor/limine/limine-bios.sys", "./kernel/vendor/limine/limine-bios-cd.bin",
        "./kernel/vendor/limine/limine-uefi-cd.bin",
        "./kernel/limine.cfg"
    )) return false;
    nob_log(NOB_INFO, "Copied limine");
    return true;
}
typedef struct {
    char* exe;
    int argc;
    char** argv;
    // Building
    bool forced;
    // Qemu
    bool gdb;
    bool cpumax;
    bool telmonitor;
} Build;
void eat_arg(Build* b, size_t arg) {
    assert(b->argc);
    assert(arg < b->argc);
    size_t total = b->argc--;
    memmove(b->argv+arg, b->argv+arg+1, total-arg-1);
}
typedef struct {
   const char* name;
   bool (*run)(Build*);
   const char* desc;
} Cmd;
bool help(Build* build);
bool build(Build* build);
bool run(Build* build);
bool bruh(Build* build);
bool run_bochs(Build* build);
bool bruh_bochs(Build* build);
bool gdb(Build* build);
bool telnet(Build* build);
bool disasm(Build* build);

Cmd commands[] = {
   { .name = "help"       , .run=help       , .desc="Help command that explains either what a specific subcommand does or lists all subcommands" },
   { .name = "build"      , .run=build      , .desc="Build the kernel and make iso" },
   { .name = "run"        , .run=run        , .desc="Run iso using qemu" },
   { .name = "bruh"       , .run=bruh       , .desc="Build+Run iso using qemu" },
   { .name = "run_bochs"  , .run=run_bochs  , .desc="(EXPERIMENTAL) Run iso using bochs"       },
   { .name = "bruh_bochs" , .run=bruh_bochs , .desc="(EXPERIMENTAL) Build+Run iso using bochs" },
   { .name = "gdb"        , .run=gdb        , .desc="Run gdb"                                  },
   { .name = "telnet"     , .run=telnet     , .desc="Run telnet"                               },
   { .name = "disasm"     , .run=disasm     , .desc="Disassemble kernel source code"           },
};

bool help(Build* build) {
    const char* what = shift_args(&build->argc, &build->argv);
    if(what) {
        for(size_t i = 0; i < NOB_ARRAY_LEN(commands); ++i) {
             if(strcmp(commands[i].name, what) == 0) {
                 nob_log(NOB_INFO, "%s: %s",what,commands[i].desc);
                 return true; 
             }
        }
        nob_log(NOB_ERROR, "Unknown subcommand: %s",what);
        return false;
    }
    nob_log(NOB_INFO, "%s <subcommand>", build->exe);
    nob_log(NOB_INFO, "List of subcommands:");
    for(size_t i = 0; i < NOB_ARRAY_LEN(commands); ++i) {
        nob_log(NOB_INFO, "  %s: %s",commands[i].name,commands[i].desc);
    }
    return true;
}
bool simple_link(const char* obj, const char* result, const char* link_script) {
    nob_log(NOB_INFO, "Linking %s",obj);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, LD);
#ifdef LDFLAGS
    nob_cmd_append(&cmd, LDFLAGS);
#endif
    nob_cmd_append(&cmd, "-T", link_script, "-o", result);
    nob_cmd_append(&cmd, obj);
    if(!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    nob_log(NOB_INFO, "Linked %s",result);
    return true;
}
bool find_libc_core(Nob_File_Paths* paths) {
    return find_objs(LIBC_TARGET_DIR, paths);
}
bool find_libc_crt(Nob_File_Paths* paths) {
    return find_objs(LIBC_CRT_TARGET_DIR, paths);
}
bool find_libc(Nob_File_Paths* paths, bool with_crt) {
    if(!find_libc_core(paths)) return false;
    if(with_crt) 
        if (!find_libc_crt(paths)) return false;
    return true;
}
bool build_nothing() {
    #define BINDIR "./bin/user/nothing/"
    #define SRCDIR "./user/nothing/"
    if(!nasm(SRCDIR "nothing.nasm", BINDIR "nothing.o")) return false;
    if(!simple_link(BINDIR "nothing.o"     , BINDIR "nothing"       , SRCDIR "link.ld")) return false;
    #undef BINDIR
    #undef SRCDIR
    return true;
}
bool build_init() {
    #define BINDIR "./bin/user/init/"
    #define SRCDIR "./user/init/src/"
    #define LIBDIR "./bin/std/"
    if(!cc_user    (SRCDIR "main.c"        , BINDIR "init.o")) return false;
    Nob_File_Paths paths = {0};
    nob_da_append(&paths, BINDIR "init.o");
    if(!find_objs(LIBDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_libc(&paths, false)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "init"  , "./user/init/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}
bool build_hello() {
    #define BINDIR "./bin/user/hello/"
    #define SRCDIR "./user/hello/src/"
    #define LIBDIR "./bin/std/"
    if(!cc_user    (SRCDIR "main.c"        , BINDIR "hello.o")) return false;
    Nob_File_Paths paths = {0};
    nob_da_append(&paths, BINDIR "hello.o");
    if(!find_objs(LIBDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_libc(&paths, true)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "hello"  , "./user/hello/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}

bool build_shell() {
    #define BINDIR "./bin/user/shell/"
    #define SRCDIR "./user/shell/src/"
    #define LIBDIR "./bin/std/"
    if(!cc_user    (SRCDIR "main.c"        , BINDIR "shell.o")) return false;
    Nob_File_Paths paths = {0};
    nob_da_append(&paths, BINDIR "shell.o");
    if(!find_objs(LIBDIR, &paths)) {
        nob_da_free(paths);
        return false;
    }
    if(!find_libc(&paths, true)) {
        nob_da_free(paths);
        return false;
    }
    if(!ld(&paths, BINDIR "shell"  , "./user/shell/link.ld")) {
        nob_da_free(paths);
        return false;
    }
    nob_da_free(paths);
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}

bool build_libc() {
    #define BINDIR LIBC_TARGET_DIR 
    #define SRCDIR "./user/libc/src/"
    #define LIBDIR "./bin/std/"
    if(!build_user_dir(SRCDIR, BINDIR, true)) return false; 
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}

bool build_crt0() {
    #define BINDIR LIBC_CRT_TARGET_DIR 
    #define SRCDIR "./user/libc/crt/"
    #define LIBDIR "./bin/std/"
    if(!build_user_dir(SRCDIR, BINDIR, true)) return false; 
    #undef BINDIR
    #undef SRCDIR
    #undef LIBDIR
    return true;
}
bool build_user() {
    if(!build_libc()) return false;
    if(!build_crt0()) return false;
    if(!build_nothing()) return false;
    if(!build_init()) return false;
    if(!build_shell()) return false;
    if(!build_hello()) return false;
    return true; 
}
// TODO Separate these out maybe? Idk
bool build(Build* build) {
    if(!make_build_dirs()) return false;
    if(!build_std(build->forced)) return false;
    if(!build_user()) return false;
    if(!embed_fs()) return false;
    if(!build_kernel(build->forced)) return false;
    if(!link_kernel()) return false;
    if(!make_limine()) return false;
    if(!make_iso()) return false;
    return true;
}
#define TELNET_PORT 1235
#define _STRINGIFY(x) # x
#define STRINGIFY(x) _STRINGIFY(x)
bool run(Build* build) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "qemu-system-x86_64",
        //"-serial", "none",
        "-serial", "file:kernel_logs.txt",
        "--no-reboot",
        "--no-shutdown",
        "-d", "int",
        "-D", "qemu.log",
        "-smp", "2",
        "-m", "128",
        "-cdrom", "./bin/OS.iso"
    );
    if(build->cpumax) {
        nob_cmd_append(
            &cmd,
            "-cpu", "max"
        );
    }
    if(build->gdb) {
        nob_cmd_append(
            &cmd,
            "-S", "-s"
        );
    }
    if(build->telmonitor) {
        nob_cmd_append(
            &cmd,
            "-monitor","telnet:127.0.0.1:" STRINGIFY(TELNET_PORT) ",server,nowait",
        );
    }

    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool gdb(Build* build) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "gdb",
        "-tui",
        "-ex", "symbol-file ./bin/iso/kernel",
        "-ex", "target remote :1234",
        "-ex", "set disassembly-flavor intel"
    );
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}

bool telnet(Build* build) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "telnet",
        "127.0.0.1",
        STRINGIFY(TELNET_PORT),
    );
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool run_bochs(Build* build) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "bochsdbg",
        "-f", "./bochs/windows.bxrc",
        "-q",
        "-rc", "./bochs/bochs.rc"
    );
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool bruh(Build* b) {
    if(!build(b)) return false;
    if(!run(b)) return false;
    return true;
}
bool bruh_bochs(Build* b) {
    if(!build(b)) return false;
    if(!run_bochs(b)) return false;
    return true;
}
bool disasm(Build* b) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "objdump",
        "-M", "intel",
        "--disassemble",
        "./bin/iso/kernel",
    );
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc,argv);
    Build build = {0};
    build.exe = shift_args(&argc, &argv);
    assert(build.exe && "First argument should be program itself");
    const char* cmd = shift_args(&argc, &argv);
    const char* arg = NULL;
    build.argc = argc;
    build.argv = argv;
    if(cmd == NULL) {
        nob_log(NOB_ERROR, "Expected subcommand but found nothing!");
        help(&build);
        return 1;
    }
    while((arg = shift_args(&build.argc, &build.argv))) {
        if(strcmp(arg, "-gdb")==0) {
            build.gdb = true;
        } else if (strcmp(arg, "--cpu=max") == 0) {
            build.cpumax = true;
        } else if (strcmp(arg, "-f") == 0) {
            build.forced = true;
        } else if (
            strcmp(arg, "-tm") == 0 ||
            strcmp(arg, "-telnet") == 0 ||
            strcmp(arg, "-telmonitor") == 0
        ) {
            build.telmonitor = true;
        } else {
            nob_log(NOB_ERROR, "Unknown argument: %s", arg);
            return 1;
        }
    }
    for(size_t i = 0; i < NOB_ARRAY_LEN(commands); ++i) {
        if(strcmp(commands[i].name,cmd) == 0) {
            if(!commands[i].run(&build)) return 1;
            return 0;
        }
    }
    nob_log(NOB_ERROR, "Unknown subcommand %s", cmd);
    return 1;
}
