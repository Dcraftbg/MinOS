#include "depan.h"
const char* strltrim(const char* data) {
    while(data[0] && isspace(data[0])) data++;
    return data;
}
void remove_backslashes(char* data) {
    char* backslash;
    // NOTE: Assumes strchr returns NULL on not found
    while((backslash=strchr(data, '\\'))) {
        switch(backslash[1]) {
        case '\n':
            memmove(backslash, backslash+2, strlen(backslash+2)+1);
            break;
        default:
            memmove(backslash, backslash+1, strlen(backslash+1)+1);
        }
        data=backslash;
    }
}
bool dep_analyse_str(char* data, char** result, Nob_File_Paths* paths) {
    // NOTE: Assumes strchr returns NULL on not found
    char* result_end = strchr(data, ':');
    if(!result_end) return false;
    result_end[0] = '\0';
    *result = data;
    data = result_end+1;
    remove_backslashes(data);
    char* lineend;
    if((lineend=strchr(data, '\n')))
        lineend[0] = '\0'; // Ignore all the stuff after the newline
    while((data=(char*)strltrim(data))[0]) {
        char* path=data;
        while(data[0] && data[0] != ' ') data++;
        nob_da_append(paths, path);
        if(data[0]) {
            data[0] = '\0';
            data++;
        }
    }
    return true;
}
