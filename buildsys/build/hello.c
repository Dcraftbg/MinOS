bool build_hello() {
    Nob_Cmd cmd = { 0 };
    bool res = go_run_nob_inside(&cmd, "user/hello");
    nob_cmd_free(cmd);
    return res;
}
