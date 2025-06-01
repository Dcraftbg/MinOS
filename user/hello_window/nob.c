#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../nob.h"


const char* sources[] = {
    "src/main.c",
};
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    char* cc = getenv("CC");
    if(!cc) cc = "cc";
    char* bindir = getenv("BINDIR");
    if(!bindir) bindir = "bin";
    char* minos_root = getenv("MINOSROOT");
    if(!minos_root) {
        nob_log(NOB_ERROR, "Missing $MINOSROOT");
        return 1;
    }
    if(!nob_mkdir_if_not_exists_silent(bindir)) return 1;
    if(!nob_mkdir_if_not_exists_silent(temp_sprintf("%s/hello_window", bindir))) return 1;
    const char* output = temp_sprintf("%s/hello_window/hello_window", bindir);
    File_Paths pathb = { 0 };
    String_Builder stb = { 0 };
    if(nob_c_needs_rebuild(&stb, &pathb, output, sources, ARRAY_LEN(sources))) {
        cmd_append(&cmd, cc, "-o", output);
        cmd_append(&cmd, temp_sprintf("-L%s/bin/libwm", minos_root));
        cmd_append(&cmd, "-lwm");
        cmd_append(&cmd, "-I../libwm/include");
        da_append_many(&cmd, sources, ARRAY_LEN(sources));
        if(!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    }
    char* rootdir = getenv("ROOTDIR");
    if(rootdir && !copy_file(output, temp_sprintf("%s/user/hello-window", rootdir))) 
        return 1;
}


