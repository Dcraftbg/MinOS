#pragma once
bool disasm(Build* build);
#define disasm_cmd subcmd(disasm, "Disassemble kernel source code")
