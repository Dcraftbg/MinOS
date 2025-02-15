#pragma once
#include <minos/fsdefs.h>
typedef inodeid_t ino_t;
enum {
    DT_UNKNOWN,
    DT_REG,
    DT_DIR,

    DT_BLK=DT_REG,
    DT_CHR=DT_REG
};
typedef unsigned char d_type_t;
struct dirent {
    ino_t d_ino;
    d_type_t d_type;
    char d_name[256];
};
typedef struct DIR DIR;
DIR* opendir(const char* name);
struct dirent* readdir(DIR *dir);
int closedir(DIR *dir);
