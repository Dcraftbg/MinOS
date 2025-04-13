bool build_ls() {
    Nob_Cmd cmd = { 0 };
    bool res = go_run_nob_inside(&cmd, "user/ls");
    nob_cmd_free(cmd);
    return res;
}
