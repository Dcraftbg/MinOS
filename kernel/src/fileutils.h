#pragma once
#include "vfs.h"
#include <stddef.h>
intptr_t read_exact(VfsFile* file, void* bytes, size_t amount);
intptr_t write_exact(VfsFile* file, const void* bytes, size_t amount);

