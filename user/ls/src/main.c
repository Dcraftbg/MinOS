#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
// strcasecmp
#include <strings.h>
#include "darray.h"

// TODO: [ ] - directory sum size in long mode (its in Kilibytes!)
// TODO: [ ] - sort by size
// TODO: [ ] - actual permissions
// TODO: [ ] - actual users
// TODO: [ ] - actual time
// TODO: [ ] - actual permissions
// TODO: [ ] - pad numbers to align them to the right (you'd need to compute the width of the size number by maybe snprintfing it)
typedef struct {
    // If necessary, stat information
    struct stat stat;
    char* name;
} Entry;
typedef struct {
    Entry* items;
    size_t len, cap;
} Entries;
int read_entire_dir(Entries* entries, const char* path) {
    DIR* dir = opendir(path);
    if(!dir) return -1;
    struct dirent* ent;
    char pathbuf[4096];
    int e = 0;
    while((ent = readdir(dir))) {
        Entry entry;
        entry.name = malloc(strlen(ent->d_name) + 1);
        if(!entry.name) {
            errno = ENOMEM;
            e = -1;
            goto end;
        }
        strcpy(entry.name, ent->d_name);
        snprintf(pathbuf, sizeof(pathbuf), path[strlen(path)-1] == '/' ? "%s%s" : "%s/%s", path, entry.name);
        if(stat(pathbuf, &entry.stat) == -1) {
            e = -1;
            goto end;
        }
        da_push(entries, entry);
    }
end:
    closedir(dir);
    return e;
}
#define LIST_ENTS_MODE_MASK 0x3
enum {
    LIST_ENTS_MODE_NONHIDDEN,
    LIST_ENTS_MODE_ALL,
    LIST_ENTS_MODE_ALMOST_ALL
};
enum {
    LIST_SIZES_1,
    LIST_SIZES_1024,
};
enum {
    SORT_ALPHABETIC,
};
typedef struct {
    uint8_t list_ents_mode;
    uint8_t list_sizes;
    uint8_t sort;
    bool long_listing;
} LsOpts;

int compar_ents_alphabetic(const void* a_void, const void* b_void) {
    const Entry* a = a_void;
    const Entry* b = b_void;
    return strcasecmp(a->name, b->name);
}
char* str_filesize(char* buf, size_t size, uint8_t list_sizes) {
    const char* filesizes = "KMGTPEZY";
    switch(list_sizes) {
    case LIST_SIZES_1: return buf + sprintf(buf, "%zu", size) + 1;
    case LIST_SIZES_1024: {
        char size_suffix = 0;
        size_t size_whole = 0, size_decimal = 0;
        while(*filesizes) {
            if(size < 1024) break;
            size_suffix = *filesizes++;
            size_decimal = ((size % 1024) * 10) / 1024;
            size_whole = size /= 1024;
        }
        buf += sprintf(buf, "%zu", size_whole);
        
        if(size_decimal) {
            *buf++ = ',';
            *buf++ = size_decimal + '0';
        }
        if(size_suffix) *buf++ = size_suffix;
        *buf++ = '\0';
    } break;
    default:
        *buf = '\0';
    }
    return buf;
}
int ls(Entries* entries, const char* path, LsOpts opts) {
    int e = read_entire_dir(entries, path);
    if(e == -1) return -1;
    int (*sort_func)(const void*, const void*) = NULL;
    switch(opts.sort) {
    case SORT_ALPHABETIC:
        sort_func = compar_ents_alphabetic;
        break;
    }
    if(sort_func) qsort(entries->items, entries->len, sizeof(Entry), sort_func);
    // a size so huge that you can't possibly have this many
    char size_fmt_buf[64];
    size_t size_pad = 0;
    if(opts.long_listing) {
        size_t dir_size_sum = 0;
        for(size_t i = 0; i < entries->len; ++i) {
            Entry* entry = &entries->items[i];
            dir_size_sum += entry->stat.st_size;
            size_t len = str_filesize(size_fmt_buf, entry->stat.st_size, opts.list_sizes) - size_fmt_buf - 1;
            if(len > size_pad) size_pad = len;
        }
        if(opts.list_sizes == LIST_SIZES_1) dir_size_sum /= 1024;
        str_filesize(size_fmt_buf, dir_size_sum, opts.list_sizes);
        printf("total %s\n", size_fmt_buf);
    }
    for(size_t i = 0; i < entries->len; ++i) {
        Entry* entry = &entries->items[i];
        switch(opts.list_ents_mode) {
        case LIST_ENTS_MODE_NONHIDDEN:
            if(*entry->name == '.') continue;
            break;
        case LIST_ENTS_MODE_ALMOST_ALL:
            if(strcmp(entry->name, "..") == 0 || strcmp(entry->name, ".") == 0) continue;
            break;
        case LIST_ENTS_MODE_ALL:
        }
        if(opts.long_listing) {
            char file_type_c = '-';
            switch(entry->stat.st_mode & S_IFMT) {
            case S_IFDIR:
                file_type_c = 'd';
                break;
            case S_IFCHR:
                file_type_c = 'c';
                break;
            }
            str_filesize(size_fmt_buf, entry->stat.st_size, opts.list_sizes);
            printf("%crwxrwxrwx 777 root root %*s Jan  1 00:00 ", file_type_c, (int)size_pad, size_fmt_buf);
        }
        // Printing name
        bool has_color = true;
        switch(entry->stat.st_mode & S_IFMT) {
        case S_IFDIR:
            printf("\033[36m");
            break;
        case S_IFCHR:
            printf("\033[93m");
            break;
        default:
            has_color = false;
        }
        printf("%s", entry->name);
        if(has_color) printf("\033[0m");

        if(opts.long_listing) printf("\n");
        else printf(" ");
    }
    if(!opts.long_listing) printf("\n");
    return 0;
}
void drop_entry(Entry* entry) {
    free(entry->name);
    entry->name = NULL;
}
void reset_entries(Entries* entries) {
    for(size_t i = 0; i < entries->len; ++i) {
        drop_entry(&entries->items[i]);
    }
    entries->len = 0;
}
typedef struct {
    const char** items;
    size_t len, cap;
} Paths;
void usage(FILE* sink, const char* exe) {
    fprintf(sink, "%s [OPTIONS] [FILE PATHS]\n", exe);
}
const char* shift_args(int *argc, const char ***argv) {
    assert((*argc) > 0);
    return ((*argc)--, *((*argv)++));
}
bool parse_opt(LsOpts* opts, char opt) {
    switch(opt) {
    case 'l':
        opts->long_listing = true;
        break;
    case 'a':
        opts->list_ents_mode = LIST_ENTS_MODE_ALL;
        break;
    case 'A':
        opts->list_ents_mode = LIST_ENTS_MODE_ALMOST_ALL;
        break;
    case 'h':
        opts->list_sizes = LIST_SIZES_1024;
        break;
    default:
        return false;
    }
    return true;
}
int main(int argc, const char** argv) {
    const char* exe = shift_args(&argc, &argv);
    Entries entries = { 0 };
    LsOpts opts = { 0 };
    Paths paths = { 0 };

    while(argc) {
        const char* arg = shift_args(&argc, &argv);
        if(*arg == '-') {
            arg++;
            while(*arg) {
                if(!parse_opt(&opts, *arg)) {
                    fprintf(stderr, "%s: invalid option -- '%c'\n", exe, *arg);
                    return 1;
                }
                arg++;
            }
        } else {
            da_push(&paths, arg);
        }
    }
    if(paths.len == 0) da_push(&paths, "./");
    for(size_t i = 0; i < paths.len; ++i) {
        if(paths.len > 1) printf("%s:\n", paths.items[i]);
        // FIXME: technically not POSIX compliant. We need to do stat() first to determine the type anyway
        // and then we can print the thing above
        if(ls(&entries, paths.items[i], opts) < 0) {
            fprintf(stderr, "%s: cannot access '%s': %s\n", exe, paths.items[i], strerror(errno));
            continue;
        }
        reset_entries(&entries);
    }
    return 0;
}
