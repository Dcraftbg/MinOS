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
