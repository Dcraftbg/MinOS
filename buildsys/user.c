bool build_user(void) {
    Nob_Cmd cmd = { 0 };
    bool ready = go_run_nob_inside(&cmd, "user", NULL, 0);
    nob_cmd_free(cmd);
    return ready; 
}
// bool go_run_inside(Nob_Cmd* cmd, const char* dir) {
//     const char* cur = nob_get_current_dir_temp();
//     assert(cur);
//     cur = nob_temp_realpath(cur);
//     if(!nob_set_current_dir(dir)) return false;
//     if(!nob_set_current_dir(dir)) return false;
//     bool res = nob_cmd_run_sync_and_reset(cmd);
//     assert(nob_set_current_dir(cur));
//     return res;
// }

// TODO: cleanup temp on failure
bool go_run_nob_inside(Nob_Cmd* cmd, const char* dir, const char** argv, size_t argc) {
    size_t temp = nob_temp_save();
    const char* cur = nob_get_current_dir_temp();
    assert(cur);
    cur = nob_temp_realpath(cur);
    if(!nob_set_current_dir(dir)) return false;
    if(nob_file_exists("nob") != 1) {
        nob_cmd_append(cmd, NOB_REBUILD_URSELF("nob", "nob.c"));
        if(!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
    nob_cmd_append(cmd, "./nob");
    nob_da_append_many(cmd, argv, argc);
    bool res = nob_cmd_run_sync_and_reset(cmd);
    assert(nob_set_current_dir(cur));
    nob_temp_rewind(temp);
    return res;
}
