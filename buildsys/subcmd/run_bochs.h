#pragma once
bool run_bochs(Build* build);
#define run_bochs_cmd subcmd(run_bochs, "(EXPERIMENTAL) Run iso using bochs")

