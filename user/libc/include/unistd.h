#pragma once
#include <sys/types.h>
#define STDOUT_FILENO 0
#define STDIN_FILENO  0
#define STDERR_FILENO 0
#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif
int close(int fd);
int open(const char* path, int flags, ...);
ssize_t write(int fd, const void* buf, size_t size);
ssize_t read(int fd, void* buf, size_t size);
off_t lseek(int fd, off_t offset, int whence);
pid_t fork(void);
void _exit(int status);
int chdir(const char* path);
char* getcwd(char* buf, size_t cap);
int ftruncate(int fd, off_t size);
int truncate(const char* path, off_t size);
