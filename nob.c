#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"

#include "config.h"
#define TELNET_PORT 1235 

typedef struct {
    char* exe;
    int argc;
    char** argv;
    // Qemu
    bool gdb;
    bool cpumax;
    bool telmonitor;
    bool nographic;
    bool kvm;
    bool uefi;
} Build;

static bool go_run_nob_inside(Nob_Cmd* cmd, const char* dir) {
    size_t temp = nob_temp_save();
    const char* cur = nob_get_current_dir_temp();
    assert(cur);
    cur = nob_temp_realpath(cur);
    if(!nob_set_current_dir(dir)) return false;
    if(nob_file_exists("nob") != 1) {
        nob_cmd_append(cmd, NOB_REBUILD_URSELF("nob", "nob.c"));
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
    nob_cmd_append(cmd, "./nob");
    bool res = nob_cmd_run_sync_and_reset(cmd);
    assert(nob_set_current_dir(cur));
    nob_temp_rewind(temp);
    return res;
}
static bool make_iso(Cmd* cmd) {
    cmd_append(
        cmd,
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
    return cmd_run_sync_and_reset(cmd);
}
static bool ustar_zip(Cmd* cmd, const char* dir, const char* result) {
    cmd_append(
        cmd,
        "tar", 
        "--create", nob_temp_sprintf("--file=%s", result),
        "--format=ustar",
        "-C", dir,
        "."
    );
    return cmd_run_sync_and_reset(cmd);
}
static bool build(Build*, Cmd* cmd) {
    if(!nob_mkdir_if_not_exists_silent("bin")) return false;
    if(!nob_mkdir_if_not_exists_silent("initrd")) return false;
    setenv("BINDIR"   , nob_temp_realpath("bin"), 1);
    setenv("CC"       , strcmp(GCC, "./gcc/bin/x86_64-elf-gcc") == 0 ? nob_temp_realpath(GCC) : GCC, 1);
    setenv("LD"       , strcmp(LD, "./gcc/bin/x86_64-elf-ld") == 0 ? nob_temp_realpath(LD) : LD, 1);
    setenv("KROOT"    , nob_temp_realpath("kernel"), 1);
    setenv("MINOSROOT", nob_get_current_dir_temp(), 1);
    if(!go_run_nob_inside(cmd, "kernel")) return false;
    if(!go_run_nob_inside(cmd, "user")) return false;
    if(!ustar_zip(cmd, "initrd", "bin/iso/initrd")) return false;
    return make_iso(cmd);
}
static bool run(Build* build, Cmd* cmd) {
    cmd_append(
        cmd,
        "qemu-system-x86_64",
        "-smp", "2",
        "-m", "128",
        "-cdrom", "./bin/OS.iso"
    );
    if(build->uefi) {
        const char* ovmf = getenv("OVMF");
        nob_log(NOB_INFO, "OVMF: %s", ovmf);
        if(!ovmf) {
            nob_log(NOB_WARNING, "Missing ovmf. Please specify the environmental variable OVMF=<path to OVMF>");
        } else {
            cmd_append(
                cmd,
                "-bios", ovmf 
            );
        }
    }
    if(build->nographic) cmd_append(cmd, "-nographic");
    else cmd_append(cmd, "-serial", "stdio");
    if(build->kvm) cmd_append(cmd, "-accel", "kvm");
    if(build->cpumax) cmd_append(cmd, "-cpu", "max");
    if(build->gdb) cmd_append(cmd, "-S", "-s");
    if(build->telmonitor) {
        int port = TELNET_PORT;
        cmd_append(
            cmd,
            "-monitor", temp_sprintf("telnet:127.0.0.1:%d,server,nowait", port)
        );
    }
    return cmd_run_sync_and_reset(cmd);
}
static bool bruh(Build* b, Cmd* cmd) {
    if(build(b, cmd) && run(b, cmd)) return true;
    return false;
}
static bool gdb(Build*, Cmd* cmd) {
    cmd_append(cmd, "gdb", "-x", "script.gdb");
    return cmd_run_sync_and_reset(cmd);
}
static bool telnet(Build*, Cmd* cmd) {
    cmd_append(cmd, "telnet", "127.0.0.1", temp_sprintf("%d", TELNET_PORT));
    return cmd_run_sync_and_reset(cmd);
}
static bool disasm(Build*, Cmd* cmd) {
    cmd_append(cmd, "objdump", "-M", "intel", "--disassemble", "./bin/iso/kernel");
    return cmd_run_sync_and_reset(cmd);
}
struct {
    const char* name;
    bool (*run)(Build*, Cmd*);
    const char* desc;
} subcmds[] = {
    { "bruh"  , bruh  , "Build + Run iso with qemu" },
    { "build" , build , "Build the OS and make an iso"},
    { "run"   , run   , "Run iso using qemu" },
    { "disasm", disasm, "Disassemble the kernel binary" },
    { "gdb"   , gdb   , "Run gdb" },
    { "telnet", telnet, "Run telnet for remote debugging" },
};

static void help(const char* exe) {
    int align = 0;
    for(size_t i = 0; i < ARRAY_LEN(subcmds); ++i) {
        if(align < strlen(subcmds[i].name)) align = strlen(subcmds[i].name);
    }
    fprintf(stderr, "%s <subcmd> ... (args)\n", exe);
    for(size_t i = 0; i < ARRAY_LEN(subcmds); ++i) {
        fprintf(stderr, "%*s - %s\n", align, subcmds[i].name, subcmds[i].desc);
    }
}
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Build build = { 0 };
    build.exe = shift_args(&argc, &argv),
    build.argc = argc;
    build.argv = argv;
    if(argc <= 0) {
        nob_log(NOB_ERROR, "Missing subcommand!");
        help(build.exe);
        return 1;
    }
    const char* subcmd = shift_args(&argc, &argv);
    for(int i = 0; i < argc; ++i) {
        if(strcmp(subcmd, "run") == 0 || strcmp(subcmd, "bruh") == 0) {
            const char* arg = argv[i];
            if(strcmp(arg, "-gdb") == 0) build.gdb = true;
            else if(strcmp(arg, "-cpu=max") == 0) build.cpumax = true;
            else if(strcmp(arg, "-telnet") == 0) build.telmonitor = true;
            else if(strcmp(arg, "-nographic") == 0) build.nographic = true;
            else if(strcmp(arg, "-kvm") == 0) build.kvm = true;
            else if(strcmp(arg, "-uefi") == 0) build.uefi = true;
            else {
                nob_log(NOB_ERROR, "Unexpected argument: `%s`", argv[i]);
                return 1;
            }
        } else {
            nob_log(NOB_ERROR, "Unexpected argument: `%s`", argv[i]);
            return 1;
        }
    }
    Cmd cmd = { 0 };
    for(size_t i = 0; i < ARRAY_LEN(subcmds); ++i) {
        if(strcmp(subcmd, subcmds[i].name) == 0) {
            return subcmds[i].run(&build, &cmd) ? 0 : 1;
        }
    }
    nob_log(NOB_ERROR, "Unknown subcommand: `%s`", subcmd);
    help(build.exe);
    return 1;
}
