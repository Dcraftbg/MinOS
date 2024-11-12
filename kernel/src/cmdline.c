#include "cmdline.h"
#include "hashutils.h"
#include "string.h"
#include "utils.h"
#include "bootutils.h"
#include "log.h"
typedef struct {
    const char* name;
    char* value;
} CmdLineParam;
CmdLineParam cmdline_params[MAX_PARAMS]={{NULL, NULL}};
size_t cmdline_params_len=0;
char* cmdline_get(const char* name) {
    size_t hash = dbj2(name) % MAX_PARAMS;
    size_t i = hash;
    while(i+1 != hash) {
        if(cmdline_params[i].name && strcmp(cmdline_params[i].name, name) == 0) return cmdline_params[i].value;
        i = (i+1)%MAX_PARAMS;
    }
    return NULL;
}
bool  cmdline_set(const char* name, char* value) {
    if(cmdline_params_len==MAX_PARAMS) return false;
    static_assert(MAX_PARAMS > 0, "MAX_PARAMS may not be 0");
    size_t hash = dbj2(name) % MAX_PARAMS;
    while(cmdline_params[hash].name) hash = (hash+1)%MAX_PARAMS;
    cmdline_params[hash] = (CmdLineParam){ name, value };
    cmdline_params_len++;
    return true;
}
void init_cmdline() {
    char* cmdline = get_kernel_cmdline();
    char* endcmdline = cmdline+strlen(cmdline);
    if(!cmdline) return;
    while(cmdline < endcmdline) {
        while(cmdline[0]==' ') cmdline++;
        char* end = strchr(cmdline, ' ');
        char* split = strchr(cmdline, '=');
        if(split >= end) {
            kwarn("Commandline malformed. Every command line option must folllow `something=value`");
            goto end_loop;
        }
        end[0] = '\0';
        split[0] = '\0';

        const char* name = cmdline;
        char* value = split+1;
        kinfo("Adding: %s, %s", name, value);
        kinfo("HELLO???");
        if(!cmdline_set(name, value)) {
            kwarn("Ignoring %s=%s because we have exceeded the max arguments", name, value);
        }
    end_loop:
        cmdline=end+1;
    }
}
