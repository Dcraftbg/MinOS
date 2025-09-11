#pragma once
/*
intptr_t devfs_get_dir_entries(Inode* dir, DirEntry* entries, size_t size, off_t offset, size_t* read_bytes);
intptr_t devfs_find(Inode* dir, const char* name, size_t namelen, Inode** inode);
#define DEVFS_OPS \
    .get_dir_entries = devfs_get_dir_entries \
    .find = devfs_find 

typedef struct Device {
    struct list_head list;
} Device;
*/
