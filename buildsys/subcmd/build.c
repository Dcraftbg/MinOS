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
    if(build->build_what.count > 0) {
        for(size_t i = 0; i < build->build_what.count; ++i) {
            const char* what = build->build_what.items[i];
            if(strcmp(what, "std") == 0) {
                if(!build_std(build->forced)) return false;
            } else if(strcmp(what, "initrd") == 0) {
                if(!initrd_setup()) return false;
            } else if (strcmp(what, "limine") == 0) {
                if(!make_limine()) return false;
            } else if(strcmp(what, "iso") == 0) {
                if(!make_limine()) return false;
                if(!make_iso()) return false;
            } else if (strcmp(what, "user") == 0) {
                if(!build_user(NULL)) return false;
            } else if(strstarts(what, "user/")) {
                if(!build_user(what+5)) return false;
            } else {
                nob_log(NOB_ERROR, "Don't know how to build `%s`", what);
                return false;
            }
        }
    } else {
        if(!build_std(build->forced)) return false;
        if(!build_user(NULL)) return false;
        if(!embed_fs()) return false;
        if(!initrd_setup()) return false;
        Nob_Cmd cmd = { 0 };
        setenv("BINDIR", "../bin", 1);
        setenv("CC", strcmp(GCC, "./gcc/bin/x86_64-elf-gcc") == 0 ? "."GCC : GCC, 1);
        setenv("LD", strcmp(LD, "./gcc/bin/x86_64-elf-ld") == 0 ? "."LD : LD, 1);
        go_run_nob_inside(&cmd, "kernel", NULL, 0);
        if(!make_iso()) return false;
    }
    return true;
}
