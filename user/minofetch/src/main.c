#include <stdio.h>
#include <string.h>
#include <minos/sysstd.h>
#include <minos/sysctl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define KiB ((long)1024)
#define MiB ((long)1024 * KiB)
char empty_icon[] = "";
size_t max_icon_width = 0;
char* icon;
const char* icon_head;
void icon_row(void) {
    size_t icon_x = 0;
    while(*icon_head && *icon_head != '\n') {
        putchar(*icon_head++);
        icon_x++;
    }
    if(*icon_head) icon_head++;
    while(icon_x < max_icon_width) {
        putchar(' ');
        icon_x++;
    }
}
void vopt(const char* fmt, va_list args) {
    icon_row();
    vprintf(fmt, args);
}
void opt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vopt(fmt, args);
    va_end(args);
}
ssize_t read_entire_file(const char* path, char** bytes, size_t* len) {
    FILE* f = fopen(path, "rb");
    if(!f) goto fopen_err; 
    if(fseek(f, 0, SEEK_END) < 0) goto fseek_err;
    long n = ftell(f);
    if(n < 0) goto ftell_err;
    if(fseek(f, 0, SEEK_SET) < 0) goto fseek_err;
    *len = n;
    char* buf;
    *bytes = buf = malloc(n + 1);
    assert(buf && "Ran out of memory LOL");
    buf[n] = '\0';
    if(fread(buf, n, 1, f) != 1) goto fread_err;
    return 0;
fread_err:
    free(buf);
    *bytes = NULL;
    *len = 0;
ftell_err:
fseek_err:
    fclose(f);
fopen_err:
    return errno;
}
int main() {
    size_t len;
    const char* icon_path = "/minos.ansi";
    long err = read_entire_file(icon_path, &icon, &len);
    if(err) {
        icon = empty_icon;
    } else {
        icon_head = icon;
        while(*icon_head) {
            const char* start = icon_head;
            while(*icon_head && *icon_head != '\n') icon_head++;
            size_t width = icon_head - start;
            if(*icon_head) icon_head++;
            if(width > max_icon_width) max_icon_width = width;
        }
        max_icon_width += 2;
    }
    icon_head = icon;
    char namebuf[MAX_SYSCTL_NAME];
    intptr_t e = _sysctl(SYSCTL_KERNEL_NAME, namebuf);
    if(e < 0) strcpy(namebuf, "Unknown");
    SysctlMeminfo meminfo = { 0 };
    e = _sysctl(SYSCTL_MEMINFO, &meminfo);
    if(e < 0) {
        meminfo.total = 0;
        meminfo.free = 0;
    }
    opt("OS: %s\n", namebuf);
    opt("Memory: %ld / %ld MiB\n", (meminfo.total - meminfo.free) / MiB, meminfo.total / MiB);
    opt("\n");
    icon_row();
    for(size_t i = 0; i < 8; ++i) {
        printf("\033[%zum   ", 40 + i);
    }
    printf("\033[0m\n");
    icon_row();
    for(size_t i = 0; i < 8; ++i) {
        printf("\033[%zum   ", 100 + i);
    }
    printf("\033[0m\n");
    printf("%s", icon_head);
    return 0;
}
