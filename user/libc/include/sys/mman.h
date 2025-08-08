#pragma once
#include <stddef.h>
#include <sys/types.h>
#include <minos/mmap.h>
#define MAP_FAILED ((void*)-1)
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
