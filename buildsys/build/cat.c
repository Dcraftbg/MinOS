bool build_cat() {
    Nob_Cmd cmd = { 0 };
    bool res = go_run_nob_inside(&cmd, "user/cat");
    nob_cmd_free(cmd);
    return res;
}
