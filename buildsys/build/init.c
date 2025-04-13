bool build_init() {
    Nob_Cmd cmd = { 0 };
    bool res = go_run_nob_inside(&cmd, "user/init");
    nob_cmd_free(cmd);
    return res;
}
