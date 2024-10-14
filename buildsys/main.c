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
#include "flags.h"
#include "utils.h"
#include "compile.h"
#include "libc.h"
#include "embed.h"
#include "build/make_dirs.h"
#include "build/crt.h"
#include "build/libc.h"
#include "build/hello.h"
#include "build/shell.h"
#include "build/nothing.h"
#include "build/std.h"
#include "build/kernel.h"
#include "build/init.h"
#include "user.h"
#include "link_kernel.h"

bool clean() {
    if(nob_file_exists("./bin/kernel")) {
        if (!remove_objs("./bin/kernel")) return false;
    }
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

#include "utils.c"
#include "embed.c"
#include "compile.c"
#include "libc.c"
#include "build/make_dirs.c"
#include "build/crt.c"
#include "build/libc.c"
#include "build/hello.c"
#include "build/shell.c"
#include "build/nothing.c"
#include "build/std.c"
#include "build/kernel.c"
#include "build/init.c"
#include "user.c"
#include "link_kernel.c"
