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
// TODO: Consider moving to ../config.h
#define CFLAGS "-g", "-nostdlib", "-march=x86-64", "-ffreestanding", "-static", "-Werror", "-Wno-unused-function", "-Wall", "-fomit-frame-pointer", "-fno-builtin", "-fno-stack-protector", "-mno-red-zone", "-mno-mmx", "-mno-sse", "-mno-sse2", "-mno-3dnow", "-fPIC", "-I", "libs/std/include", "-I", "kernel/src"

#define LDFLAGS "-g"

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
const char* get_base(const char* path) {
    const char* end = path;
    while(*end) end++;
    while(end >= path) {
        if(*end == '/' || *end == '\\') return end+1;
        end--;
    }
    return end;
}
char* shift_args(int *argc, char ***argv) {
    if((*argc) <= 0) return NULL;
    char* arg = **argv;
    (*argc)--;
    (*argv)++;
    return arg;
}
const char* strip_prefix(const char* str, const char* prefix) {
     size_t len = strlen(prefix);
     if(strncmp(str, prefix, strlen(prefix))==0) return str+len;
     return NULL;
}
bool nob_mkdir_if_not_exists_silent(const char *path) {
     if(nob_file_exists(path)) return true;
     return nob_mkdir_if_not_exists(path);
}
bool make_build_dirs() {
    if(!nob_mkdir_if_not_exists_silent("./bin"             )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/kernel"      )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/std"         )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/iso"         )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user"        )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/nothing")) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/syscall_test")) return false;
    return true;
}
bool remove_objs(const char* dirpath) {
   DIR *dir = opendir(dirpath);
   if (dir == NULL) {
       nob_log(NOB_ERROR, "Could not open directory %s: %s",dirpath,strerror(errno));
       return false;
   }
   errno = 0;
   struct dirent *ent = readdir(dir);
   while(ent != NULL) {
        const char* fext = get_ext(ent->d_name);
        const char* path = nob_temp_sprintf("%s/%s",dirpath,ent->d_name); 
        Nob_File_Type type = nob_get_file_type(path);
        if(strcmp(fext, "o")==0) {
            if(type == NOB_FILE_REGULAR) {
               if(!nob_delete_file(path)) {
                  closedir(dir);
                  return false;
               }
            }
        }
        if (type == NOB_FILE_DIRECTORY) {
            Nob_String_Builder sb = {0};
            nob_sb_append_cstr(&sb, path);
            nob_sb_append_null(&sb);
            if(!remove_objs(sb.items)) {
                nob_sb_free(sb);
                return false;
            }
            nob_sb_free(sb);
        }
        ent = readdir(dir);
   }
   if (dir) closedir(dir);
   return true;
}
bool clean() {
    if(nob_file_exists("./bin/kernel")) {
        if (!remove_objs("./bin/kernel")) return false;
    }
    return true;
}
typedef struct {
    const char* name;
    size_t offset;
    size_t size;
    size_t kind;
} EmbedEntry;
typedef enum {
    EMBED_DIR,
    EMBED_FILE
} EmbedFsKind;
typedef struct {
    EmbedEntry* items;
    size_t count;
    size_t capacity;
    Nob_String_Builder data;
} EmbedFs;
bool embed_mkdir(EmbedFs* fs, const char* fspath) {
    EmbedEntry entry = {
        fspath,
        0,
        0,
        EMBED_DIR,
    };
    nob_da_append(fs, entry);
    return true;
}
bool embed(EmbedFs* fs, const char* file, const char* fspath) {
    Nob_String_Builder sb={0};
    if(!nob_read_entire_file(file, &sb)) return false;
    EmbedEntry entry = {
        fspath,
        fs->data.count,
        sb.count,
        EMBED_FILE, 
    };
    nob_sb_append_buf(&fs->data, sb.items, sb.count);
    nob_da_append(fs, entry);
    nob_sb_free(sb);
    return true;
}
void embed_fs_clean(EmbedFs* fs) {
    nob_sb_free(fs->data);
    nob_da_free(*fs);
} 
bool embed_fs() {
    bool result = true;
    EmbedFs fs = {0};
    if(!embed_mkdir(&fs, "/user")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/nothing/nothing", "/user/nothing")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/syscall_test/syscall_test", "/user/syscall_test")) nob_return_defer(false);
    const char* opath = "./kernel/embed.h";
    FILE* f = fopen(opath, "wb");
    if(!f) {
        nob_log(NOB_ERROR, "Failed to open file %s: %s", opath, strerror(errno));
        embed_fs_clean(&fs);
        return false;
    }
    fprintf(f, "#pragma once\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#include <stddef.h>\n");
    fprintf(f, "typedef struct {\n");
    fprintf(f, "    const char* name;\n");
    fprintf(f, "    size_t offset;\n");
    fprintf(f, "    size_t size;\n");
    fprintf(f, "    size_t kind;\n");
    fprintf(f, "} EmbedEntry;\n");
    fprintf(f, "typedef enum {\n");
    fprintf(f, "    EMBED_DIR,\n");
    fprintf(f, "    EMBED_FILE\n");
    fprintf(f, "} EmbedFsKind;\n");
    fprintf(f, "size_t embed_entries_count = %zu;\n", fs.count);
    fprintf(f, "EmbedEntry embed_entries[] = {\n");
    for(size_t i = 0; i < fs.count; ++i) {
        if(i != 0) fprintf(f, ",\n");
        EmbedEntry* e = &fs.items[i];
        fprintf(f, "   { \"%s\", %zu, %zu, %zu }",e->name,e-> offset,e->size,e->kind);
    }
    fprintf(f, "\n};\n");
    fprintf(f, "size_t embed_data_size = %zu;\n",fs.data.count);
    fprintf(f, "uint8_t embed_data[] = {");
    for(size_t i = 0; i < fs.data.count; ++i) {
        if(i != 0) {
            fprintf(f, ", ");
        }
        if(i % 8 == 0) fprintf(f, "\n   ");
        uint8_t byte = fs.data.items[i];
        fprintf(f, "0x%02X", byte);
    }
    fprintf(f, "\n};");
defer:
    embed_fs_clean(&fs);
    fclose(f);
    return result;
}
// TODO: cc but async
bool cc(const char* ipath, const char* opath) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, GCC);
    nob_cmd_append(&cmd, CFLAGS);
    nob_cmd_append(&cmd, "-I", "./kernel/vendor/limine");
    nob_cmd_append(&cmd, "-I", "./kernel/vendor/stb");
    nob_cmd_append(&cmd, "-c", ipath, "-o", opath);
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}
// TODO: nasm but async
bool nasm(const char* ipath, const char* opath) {
    Nob_Cmd cmd = {0};
    const char* end = get_base(ipath);
    nob_cmd_append(&cmd, "nasm");
    if(end) {
        Nob_String_View sv = {0};
        sv.data = ipath;
        sv.count = end-ipath;
        nob_cmd_append(&cmd, "-I", nob_temp_sv_to_cstr(sv));
    }
    nob_cmd_append(&cmd, "-f", "elf64", ipath, "-o", opath);
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool _build_kernel_dir(const char* rootdir, const char* build_dir, const char* srcdir, bool forced) {
   bool result = true;
   Nob_String_Builder opath = {0};
   DIR *dir = opendir(srcdir);
   if (dir == NULL) {
       nob_log(NOB_ERROR, "Could not open directory %s: %s",srcdir,strerror(errno));
       return false;
   }
   errno = 0;
   struct dirent *ent = readdir(dir);
   while(ent != NULL) {
        const char* fext = get_ext(ent->d_name);
        const char* path = nob_temp_sprintf("%s/%s",srcdir,ent->d_name); 
        Nob_File_Type type = nob_get_file_type(path);

        if(type == NOB_FILE_REGULAR) {
           if(strcmp(fext, "c")==0) {
               opath.count = 0;
               nob_sb_append_cstr(&opath, build_dir);
               nob_sb_append_cstr(&opath, "/");
               const char* file = strip_prefix(path, rootdir)+1;
               Nob_String_View sv = nob_sv_from_cstr(file);
               sv.count-=2; // Remove .c
               nob_sb_append_buf(&opath,sv.data,sv.count);
               nob_sb_append_cstr(&opath, ".o");
               nob_sb_append_null(&opath);
               if((!nob_file_exists(opath.items)) || nob_needs_rebuild1(opath.items,path) || forced) {
                   if(!cc(path,opath.items)) nob_return_defer(false);
               }
           } else if(strcmp(fext, "nasm") == 0) {
               opath.count = 0;
               nob_sb_append_cstr(&opath, build_dir);
               nob_sb_append_cstr(&opath, "/");
               const char* file = strip_prefix(path, rootdir)+1;
               Nob_String_View sv = nob_sv_from_cstr(file);
               sv.count-=5; // Remove .nasm
               nob_sb_append_buf(&opath,sv.data,sv.count);
               nob_sb_append_cstr(&opath, ".o");
               nob_sb_append_null(&opath);
               if((!nob_file_exists(opath.items)) || nob_needs_rebuild1(opath.items,path) || forced) {
                   if(!nasm(path,opath.items)) nob_return_defer(false);
               }
           }
        }
        if (type == NOB_FILE_DIRECTORY && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
         
           opath.count = 0;
           nob_sb_append_cstr(&opath, build_dir);
           nob_sb_append_cstr(&opath, "/");
           nob_sb_append_cstr(&opath, strip_prefix(path, rootdir)+1);
           nob_sb_append_null(&opath);
           if(!nob_mkdir_if_not_exists_silent(opath.items)) nob_return_defer(false);
           opath.count = 0;
           nob_sb_append_cstr(&opath, path);
           nob_sb_append_null(&opath);
           if(!_build_kernel_dir(rootdir, build_dir, opath.items, forced)) nob_return_defer(false);
        }
        ent = readdir(dir);
   }
defer:
   if (dir) closedir(dir);
   if (opath.items) nob_sb_free(opath);
   return result;
}

static bool build_kernel_dir(const char* rootdir, const char* build_dir, bool forced) {
   return _build_kernel_dir(rootdir, build_dir, rootdir, forced);
}
bool build_kernel(bool forced) {
    if(!build_kernel_dir("./kernel/src"  , "./bin/kernel", forced)) return false;
    if(!build_kernel_dir("./libs/std/src", "./bin/std"   , forced)) return false;
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
bool link_kernel() {
    nob_log(NOB_INFO, "Linking kernel");
    Nob_File_Paths paths = {0};
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, LD);
#ifdef LDFLAGS
    nob_cmd_append(&cmd, LDFLAGS);
#endif
    nob_cmd_append(&cmd, "-T", "./linker/link.ld", "-o", "./bin/iso/kernel");
    if(!find_objs("./bin/kernel",&paths)) {
        nob_cmd_free(cmd);
        return false;
    }

    if(!find_objs("./bin/std",&paths)) {
        nob_cmd_free(cmd);
        return false;
    }
    for(size_t i = 0; i < paths.count; ++i) {
        nob_cmd_append(&cmd, paths.items[i]);
    }
    nob_da_free(paths);
    if(!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    nob_log(NOB_INFO, "Linked kernel successfully");
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
} Build;
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
bool disasm(Build* build);

Cmd commands[] = {
   { .name = "help"       , .run=help       , .desc="Help command that explains either what a specific subcommand does or lists all subcommands" },
   { .name = "build"      , .run=build      , .desc="Build the kernel and make iso" },
   { .name = "run"        , .run=run        , .desc="Run iso using qemu" },
   { .name = "bruh"       , .run=bruh       , .desc="Build+Run iso using qemu" },
   { .name = "run_bochs"  , .run=run_bochs  , .desc="(EXPERIMENTAL) Run iso using bochs"       },
   { .name = "bruh_bochs" , .run=bruh_bochs , .desc="(EXPERIMENTAL) Build+Run iso using bochs" },
   { .name = "gdb"        , .run=gdb        , .desc="Run gdb"                                  },
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
// TODO: Usermode CC

bool build_nothing() {
    #define BINDIR "./bin/user/nothing/"
    #define SRCDIR "./user/nothing/"
    if(!nasm(SRCDIR "nothing.nasm", BINDIR "nothing.o")) return false;
    if(!simple_link(BINDIR "nothing.o"     , BINDIR "nothing"       , SRCDIR "link.ld")) return false;
    #undef BINDIR
    #undef SRCDIR
    return true;
}
bool build_syscall_test() {
    #define BINDIR "./bin/user/syscall_test/"
    #define SRCDIR "./user/syscall_test/src/"
    if(!cc         (SRCDIR "main.c"        , BINDIR "syscall_test.o")) return false;
    if(!simple_link(BINDIR "syscall_test.o", BINDIR "syscall_test"  , "./user/syscall_test/link.ld")) return false;
    #undef BINDIR
    #undef SRCDIR
    return true;
}
bool build_user() {
    if(!build_nothing()) return false;
    if(!build_syscall_test()) return false;
    return true; 
}
// TODO Separate these out maybe? Idk
bool build(Build* build) {
    if(!make_build_dirs()) return false;
    if(!build_user()) return false;
    if(!embed_fs()) return false;
    bool forced = false;
    if(build->argc > 0 && strcmp(build->argv[0], "-f")==0) {
        forced = true;
        shift_args(&build->argc, &build->argv);
    }
    if(!build_kernel(forced)) return false;
    if(!link_kernel()) return false;
    if(!make_limine()) return false;
    if(!make_iso()) return false;
    return true;
}
bool run(Build* build) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "qemu-system-x86_64",
        //"-serial", "none",
        "-serial", "file:kernel_logs.txt",
        // "--no-reboot",
        // "--no-shutdown",
        // "-d", "int",
        "-cpu", "max",
        "-smp", "2",
        "-m", "128",
        "-cdrom", "./bin/OS.iso"
    );
    if(build->argc && strcmp(build->argv[0], "-gdb")==0) {
        nob_cmd_append(
            &cmd,
            "-S", "-s"
        );
        shift_args(&build->argc, &build->argv);
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
    build.argc = argc;
    build.argv = argv;
    if(cmd == NULL) {
        nob_log(NOB_ERROR, "Expected subcommand but found nothing!");
        help(&build);
        return 1;
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
