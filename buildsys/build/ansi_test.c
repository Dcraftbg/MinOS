bool build_ansi_test() {
    Nob_Cmd cmd = { 0 };
    bool res = go_run_nob_inside(&cmd, "user/ansi_test");
    nob_cmd_free(cmd);
    return res;
}
