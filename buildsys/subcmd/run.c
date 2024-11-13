bool run(Build* build) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(
        &cmd,
        "qemu-system-x86_64",
        //"-serial", "none",
        "--no-reboot",
        "--no-shutdown",
        "-d", "int",
        "-D", "qemu.log",
        "-smp", "2",
        "-m", "128",
        "-cdrom", "./bin/OS.iso"
    );
    if(build->nographic) {
        nob_cmd_append(
            &cmd, 
            "-nographic"
        );
    } else {
        nob_cmd_append(
            &cmd, 
            "-serial", "stdio",
        );
    }
    if(build->cpumax) {
        nob_cmd_append(
            &cmd,
            "-cpu", "max"
        );
    }
    if(build->gdb) {
        nob_cmd_append(
            &cmd,
            "-S", "-s"
        );
    }
    if(build->telmonitor) {
        nob_cmd_append(
            &cmd,
            "-monitor","telnet:127.0.0.1:" STRINGIFY(TELNET_PORT) ",server,nowait",
        );
    }

    if (!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
