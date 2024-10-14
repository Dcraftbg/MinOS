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
