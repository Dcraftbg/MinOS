// Just to split them up a bit since these guys require environ and a bunch of other crap
#include <unistd.h>
#include <environ.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

int execvpe(const char* file, char* const* argv, char *const* envp) {
    if(
        (file[0] == '/') ||
        (file[0] == '.' && file[1] == '/') ||
        (file[0] == '.' && file[1] == '.' && file[2] == '/')
    ) {
        return execve(file, argv, envp);
    }
    const char* path = getenv("PATH");
    if(!path) return (errno = ENOENT, -1);
    char pathbuf[4096];
    size_t file_len = strlen(file);

    char* end;
    for(;*path; path = end + 1) {
        end = strchr(path, ':');
        if(!(*end)) break;
        if(end == path) continue;
        if((end-path) + 1 + file_len >= sizeof(pathbuf)) continue;
        memcpy(pathbuf, path, end-path);
        pathbuf[end-path] = '\0';
        if((end-1)[0] != '/') strcat(pathbuf, "/");
        strcat(pathbuf, file);
        struct stat stats;
        if(stat(pathbuf, &stats) >= 0 && (stats.st_mode & S_IFMT) != S_IFDIR) {
            if(argv[0]) ((char**)argv)[0] = pathbuf;
            return execve(pathbuf, argv, envp);
        }
    }
    return (errno = ENOENT, -1);
}
int execv(const char* path, char* const* argv) {
    return execve(path, argv, environ);
}
int execvp(const char* file, char* const* argv) {
    return execvpe(file, argv, environ);
}
// TODO: execl functions
