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

