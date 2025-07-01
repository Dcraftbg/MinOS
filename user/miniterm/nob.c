#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../nob.h"


const char* sources[] = {
    "src/main.c",
};
char* cc;
char* bindir;
bool build_flanterm(File_Paths* pathb, String_Builder* stb, Cmd* cmd) {
    if(!nob_mkdir_if_not_exists_silent(temp_sprintf("%s/miniterm/vendor/flanterm_backends", bindir))) return 1;
    const char* flanterm_dir = "vendor/flanterm/src/";
    static const char* ft_sources[] = {
        "flanterm.c",
        "flanterm_backends/fb.c",
    };
    for(size_t i = 0; i < ARRAY_LEN(ft_sources); ++i) {
        const char* output = temp_sprintf("%s/miniterm/vendor/%.*s.o", bindir, (int)strlen(ft_sources[i])-2, ft_sources[i]);
        const char* input = temp_sprintf("%s%s", flanterm_dir, ft_sources[i]);
        if(nob_c_needs_rebuild1(stb, pathb, output, input)) {
            cmd_append(cmd, cc, "-O2", "-g", "-MD", "-DFLANTERM_FB_DISABLE_BUMP_ALLOC", "-c", input, "-o", output);
            if(!cmd_run_sync_and_reset(cmd)) return false;
        }
    }
    return true;
}
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    cc = getenv("CC");
    if(!cc) cc = "cc";
    bindir = getenv("BINDIR");
    if(!bindir) bindir = "bin";
    char* minos_root = getenv("MINOSROOT");
    if(!minos_root) {
        nob_log(NOB_ERROR, "Missing $MINOSROOT");
        return 1;
    }
    if(!nob_mkdir_if_not_exists_silent(bindir)) return 1;
    if(!nob_mkdir_if_not_exists_silent(temp_sprintf("%s/miniterm", bindir))) return 1;
    if(!nob_mkdir_if_not_exists_silent(temp_sprintf("%s/miniterm/vendor", bindir))) return 1;
    File_Paths pathb = { 0 };
    String_Builder stb = { 0 };
    if(!build_flanterm(&pathb, &stb, &cmd)) return 1;
    const char* output = temp_sprintf("%s/miniterm/miniterm", bindir);
    if(nob_c_needs_rebuild(&stb, &pathb, output, sources, ARRAY_LEN(sources))) {
        cmd_append(&cmd, cc, "-o", output);
        cmd_append(&cmd, "-O0", "-g", "-Wall", "-Werror", "-Wextra", "-Wno-unused-function");
        cmd_append(&cmd, temp_sprintf("-L%s/bin/libwm", minos_root));
        cmd_append(&cmd, "-lwm");
        cmd_append(&cmd, "-I../libwm/include");
        cmd_append(&cmd, temp_sprintf("-L%s/bin/libpluto", minos_root));
        cmd_append(&cmd, "-lpluto");
        cmd_append(&cmd, "-I../libpluto/include");
        cmd_append(&cmd, "-Ivendor/flanterm/src/");
        cmd_append(&cmd, temp_sprintf("%s/miniterm/vendor/flanterm.o", bindir));
        cmd_append(&cmd, temp_sprintf("%s/miniterm/vendor/flanterm_backends/fb.o", bindir));
        da_append_many(&cmd, sources, ARRAY_LEN(sources));
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }
    char* rootdir = getenv("ROOTDIR");
    if(rootdir && !copy_file(output, temp_sprintf("%s/user/miniterm", rootdir))) 
        return 1;
}


