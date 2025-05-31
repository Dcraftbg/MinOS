bool make_iso() {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "xorriso",
        "-as", "mkisofs",
        "-b", "limine-bios-cd.bin",
        "-no-emul-boot",
        "-boot-load-size", "4",
        "-boot-info-table",
        "--efi-boot", "limine-uefi-cd.bin",
        "-efi-boot-part",
        "--efi-boot-image",
        "./bin/iso",
        "-o",
        "./bin/OS.iso"
    );
    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool make_limine() {
    if(!copy_all_to(
        "./bin/iso",
        "./kernel/vendor/limine/limine-bios.sys", "./kernel/vendor/limine/limine-bios-cd.bin",
        "./kernel/vendor/limine/limine-uefi-cd.bin",
        "./kernel/limine.cfg"
    )) return false;
    nob_log(NOB_INFO, "Copied limine");
    return true;
}

// TODO Separate these out maybe? Idk
bool build(Build* build) {
    if(!make_build_dirs()) return false;
    setenv("BINDIR", nob_temp_realpath("bin"), 1);
    setenv("CC", strcmp(GCC, "./gcc/bin/x86_64-elf-gcc") == 0 ? nob_temp_realpath(GCC) : GCC, 1);
    setenv("LD", strcmp(LD, "./gcc/bin/x86_64-elf-ld") == 0 ? nob_temp_realpath(LD) : LD, 1);
    setenv("KROOT", nob_temp_realpath("kernel"), 1);
    setenv("MINOSROOT", nob_get_current_dir_temp(), 1);
    Nob_Cmd cmd = { 0 };
    if(!go_run_nob_inside(&cmd, "kernel", NULL, 0)) return false;
    if(!build_user()) return false;
    if(!embed_fs()) return false;
    if(!initrd_setup()) return false;
    if(!make_iso()) return false;
    return true;
}
