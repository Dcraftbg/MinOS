#pragma once
#include "../../vfs.h"
#include "../../utils.h"
#include <minos/status.h>
#define TMPFS_NAME_LIMIT 128
extern Fs tmpfs;
intptr_t tmpfs_register_device(Inode* dir, Device* device, const char* name, size_t namelen);
intptr_t tmpfs_socket_creat(Inode* parent, Socket* sock, const char* name, size_t namelen);
Socket* tmpfs_get_socket(Inode* inode);
// void tmpfs_dump_file(VfsFile* file);
