
bool build_shell() {
    Nob_Cmd cmd = { 0 };
    bool res = go_run_nob_inside(&cmd, "user/shell");
    nob_cmd_free(cmd);
    return res;
}
