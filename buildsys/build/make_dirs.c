#include "make_dirs.h"
#include "libc.h"
#include "crt.h"
bool make_build_dirs() {
    if(!nob_mkdir_if_not_exists_silent("./bin"               )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/kernel"        )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/std"           )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/iso"           )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user"          )) return false;
    if(!nob_mkdir_if_not_exists_silent(LIBC_TARGET_DIR       )) return false;
    if(!nob_mkdir_if_not_exists_silent(LIBC_CRT_TARGET_DIR   )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/nothing"  )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/init"     )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/shell"    )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/hello"    )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/cat"      )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/ansi_test")) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/ls"       )) return false;
    if(!nob_mkdir_if_not_exists_silent("./bin/user/fbtest"   )) return false;
    if(!nob_mkdir_if_not_exists_silent("./initrd"            )) return false;
    if(!nob_mkdir_if_not_exists_silent("./initrd/user/"      )) return false;
    return true;
}
