#pragma once
bool run(Build* build);
#define run_cmd subcmd(run, "Run iso using qemu")
