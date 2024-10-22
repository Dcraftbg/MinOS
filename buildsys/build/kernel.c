bool build_kernel(bool forced) {
    if(!build_kernel_dir("./kernel/src"  , "./bin/kernel", forced)) return false;
    nob_log(NOB_INFO, "Built kernel successfully");
    return true;
}
