#pragma once
#include <stdint.h>
#include <stddef.h>
#include <minos/status.h>
#define TMPFS_NAME_LIMIT 128
typedef struct Fs Fs;
extern Fs tmpfs;
typedef struct Inode Inode;
intptr_t tmpfs_register_device(Inode* dir, Inode* device, const char* name, size_t namelen);
intptr_t tmpfs_socket_creat(Inode* parent, Inode* sock, const char* name, size_t namelen);
// void tmpfs_dump_file(VfsFile* file);
