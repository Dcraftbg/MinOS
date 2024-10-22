bool build_std(bool forced) {
    if(!build_kernel_dir("./libs/std/src", "./bin/std"   , forced)) return false;
    nob_log(NOB_INFO, "Built std successfully");
    return true;
}
