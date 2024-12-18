#pragma once
#include "vfs.h"
#include <stddef.h>
intptr_t read_exact(Inode* file, void* bytes, size_t amount, off_t offset);
intptr_t write_exact(Inode* file, const void* bytes, size_t amount, off_t offset);

