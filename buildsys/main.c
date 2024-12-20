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
    Nob_File_Paths build_what;
    // Building
    bool forced;
    // Qemu
    bool gdb;
    bool cpumax;
    bool telmonitor;
    bool nographic;
    bool kvm;
    bool uefi;
} Build;
#include "flags.h"
#include "utils.h"
#include "depan.h"
#include "compile.h"
#include "libc.h"
#include "embed.h"
#include "ustar.h"
#include "build/make_dirs.h"
#include "build/crt.h"
#include "build/libc.h"
#include "build/hello.h"
#include "build/cat.h"
#include "build/ls.h"
#include "build/shell.h"
#include "build/nothing.h"
#include "build/std.h"
#include "build/kernel.h"
#include "build/init.h"
#include "build/fbtest.h"
#include "user.h"
#include "link_kernel.h"
#include "subcmd.h"
typedef struct {
   const char* name;
   bool (*run)(Build*);
   const char* desc;
} Cmd;

#define TELNET_PORT 1235
#define _STRINGIFY(x) # x
#define STRINGIFY(x) _STRINGIFY(x)
#include "subcmd.h"
#include "subcmd/help.h"
#include "subcmd/build.h"
#include "subcmd/run.h" 
#include "subcmd/bruh.h"
#include "subcmd/run_bochs.h"
#include "subcmd/bruh_bochs.h"
#include "subcmd/gdb.h"
#include "subcmd/telnet.h"
#include "subcmd/disasm.h"
Cmd commands[] = {
    help_cmd,
    build_cmd,
    run_cmd, 
    bruh_cmd,
    run_bochs_cmd,
    bruh_bochs_cmd,
    gdb_cmd,
    telnet_cmd,
    disasm_cmd,
};
bool strstarts(const char* str, const char* needle) {
    // Works? Why:
    // Because str is null terminated and it will complain about it if its less
    return memcmp(str, needle, strlen(needle)) == 0;
}
bool nob_read_entire_dir_recursively(const char* path, Nob_File_Paths* paths) {
    DIR *dir = opendir(path);
    if(!dir) {
        nob_log(NOB_ERROR, "Could not open directory `%s`", path);
        return false;
    }
    struct dirent* ent;
    while((ent=readdir(dir))) {
        nob_da_append(paths, nob_temp_sprintf("%s/%s", path, ent->d_name));
    }
    if(dir) closedir(dir);
    return true;
}
bool rebuild_urself(int argc, char** argv) {
    Nob_File_Paths paths={0};
    if(!nob_read_entire_dir_recursively("./buildsys", &paths)) return false;
    #ifdef _WIN32
        char binary_path[MAX_PATH];
        NOB_ASSERT(GetModuleFileNameA(NULL, binary_path, MAX_PATH));
    #else
        const char *binary_path = argv[0];                                                   
    #endif                                                                                     
    if(nob_needs_rebuild(binary_path, paths.items, paths.count)) {
        Nob_String_Builder sb = {0};                                                     
        nob_sb_append_cstr(&sb, binary_path);                                            
        nob_sb_append_cstr(&sb, ".old");                                                 
        nob_sb_append_null(&sb);                                                         
                                                                                         
        if (!nob_rename(binary_path, sb.items)) exit(1);                                 
        Nob_Cmd rebuild = {0};                                                           
        nob_cmd_append(&rebuild, NOB_REBUILD_URSELF(binary_path, __FILE__));          
        bool rebuild_succeeded = nob_cmd_run_sync(rebuild);                              
        nob_cmd_free(rebuild);                                                           
        if (!rebuild_succeeded) {                                                        
            nob_rename(sb.items, binary_path);                                           
            exit(1);                                                                     
        }                                                                                
                                                                                         
        Nob_Cmd cmd = {0};                                                               
        nob_da_append_many(&cmd, argv, argc);                                            
        if (!nob_cmd_run_sync(cmd)) exit(1);                                             
        exit(0);                                                                         
    }
    return true;
}
int main(int argc, char** argv) {
    assert(rebuild_urself(argc,argv));
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
        } else if (
            strcmp(arg, "-nographic") == 0
        ) {
            build.nographic = true;
        } else if (
            strcmp(arg, "-kvm") == 0
        ) {
            build.kvm = true;
        } else if (
            strcmp(arg, "-uefi") == 0
        ) {
            build.uefi = true;
        }else {
            if(
                strcmp(cmd, "build") == 0 ||
                strstarts(cmd, "bruh")
            ) {
                nob_da_append(&build.build_what, arg);
            } else {
                nob_log(NOB_ERROR, "Unknown argument: %s", arg);
                return 1;
            }
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
#include "ustar.c"
#include "depan.c"
#include "compile.c"
#include "libc.c"
#include "build/make_dirs.c"
#include "build/crt.c"
#include "build/libc.c"
#include "build/hello.c"
#include "build/cat.c"
#include "build/ls.c"
#include "build/shell.c"
#include "build/nothing.c"
#include "build/std.c"
#include "build/kernel.c"
#include "build/init.c"
#include "build/fbtest.c"
#include "user.c"
#include "link_kernel.c"
// Subcommands
#include "subcmd/help.c"
#include "subcmd/build.c"
#include "subcmd/run.c" 
#include "subcmd/bruh.c"
#include "subcmd/run_bochs.c"
#include "subcmd/bruh_bochs.c"
#include "subcmd/gdb.c"
#include "subcmd/telnet.c"
#include "subcmd/disasm.c"

