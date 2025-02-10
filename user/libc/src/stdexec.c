#include <stdexec.h>
#include <string.h>
#include <minos/status.h>
#include <stdio.h>
intptr_t execvp(const char* exe_path, const char** argv, size_t argc) {
    if(
        (exe_path[0] == '/') ||
        (exe_path[0] == '.' && exe_path[1] == '/') ||
        (exe_path[0] == '.' && exe_path[1] == '.' && exe_path[2] == '/')
    ) {
        return exec(exe_path, argv, argc);
    }
    const char* path = getenv("PATH");
    if(!path) return -NOT_FOUND;
    char pathbuf[PATH_MAX];
    size_t exe_path_len = strlen(exe_path);

    char* end;
    for(;*path; path = end + 1) {
        end = strchr(path, ':');
        if(!(*end)) break;
        if(end == path) continue;
        if((end-path) + 1 + exe_path_len >= sizeof(pathbuf)) continue;
        memcpy(pathbuf, path, end-path);
        pathbuf[end-path] = '\0';
        if((end-1)[0] != '/') strcat(pathbuf, "/");
        strcat(pathbuf, exe_path);
        Stats stats;
        if(stat(pathbuf, &stats) >= 0 && stats.kind == INODE_FILE) {
            return exec(pathbuf, argv, argc);
        }
    }
    return -NOT_FOUND;
}
