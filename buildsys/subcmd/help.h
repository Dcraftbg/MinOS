#pragma once
bool help(Build* build);
#define help_cmd subcmd(help, "Help command that explains either what a specific subcommand does or lists all subcommands")
