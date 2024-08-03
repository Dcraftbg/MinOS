#pragma once
#include "../../vfs.h"
#include "../../utils.h"
#include <status.h>
extern FsDriver tmpfs_driver;
void tmpfs_log(Inode* inode, size_t pad);
intptr_t tmpfs_register_device(VfsDir* dir, Device* device, Inode* result);
// void tmpfs_dump_file(VfsFile* file);
