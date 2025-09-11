#pragma once

#define KCC_INC(path) "-I", path
#if KLIBC
# define KCC_INCLUDE_DIRS KCC_INC("include")
#else
# define KCC_INCLUDE_DIRS KCC_INC("shared/include"), KCC_INC("src"), \
                          KCC_INC("vendor/limine"), KCC_INC("vendor/stb"), \
                          KCC_INC("klibc/include")
#endif

#define KCC_CFLAGS \
    "-nostdlib", \
    "-march=x86-64", \
    "-ffreestanding", \
    "-static", \
    "-Werror", "-Wno-unused-function", \
    "-Wno-address-of-packed-member",  \
    "-Wall", \
    "-fno-stack-protector", \
    "-fcf-protection=none",  \
    "-O2", \
    "-MMD", \
    "-MP", \
    /*"-fomit-frame-pointer", "-fno-builtin", */ \
    "-mgeneral-regs-only", \
    "-mno-mmx", \
    "-mno-sse", "-mno-sse2", \
    "-mno-3dnow", \
    "-fPIC"

#define kcc_common(cmd) cmd_append(cmd, cc, KCC_CFLAGS, KCC_INCLUDE_DIRS);
#define kcc_run_and_cleanup(cmd) \
    if(!nob_cmd_run_sync_and_reset(cmd)) { \
        char* str = nob_temp_strdup(out); \
        size_t str_len = strlen(str); \
        assert(str_len); \
        str[str_len-1] = 'd'; \
        nob_delete_file(str); \
        exit(1); \
    }
