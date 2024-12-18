#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdexec.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <environ.h>

intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    size_t i = 0;
    while(i < bufmax) {
        if((e = read(STDIN_FILENO, buf+i, 1)) < 0) {
            return e;
        }
        if(buf[i++] == '\n') return i;
    }
    return -BUFFER_TOO_SMALL;
}
static bool isspace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
}
char* trim_r(char* buf) {
    char* start = buf;
    while(*buf) buf++;
    buf--;
    while(buf >= start && isspace(buf[0])) {
        *buf = '\0';
        buf--;
    }
    return start;
}
const char* trim_l(const char* buf) {
    while(buf[0] && isspace(buf[0])) buf++;
    return buf;
}
typedef struct ArenaNode {
    struct ArenaNode* next;
    size_t len, cap;
    char data[];
} ArenaNode;
typedef struct {
    ArenaNode* node;
} Arena;
#define _STRINGIFY(x) # x
#define STRINGIFY(x) _STRINGIFY(x)
#define assert(x) do {\
        if(!(x)) {\
            fputs(__FILE__ ":" STRINGIFY(__LINE__) "Assertion failed" STRINGIFY(x), stderr);\
            exit(1);\
        }\
    } while(0)
ArenaNode* arena_node_new(size_t size) {
    size = ((size+15)/16)*16;
    ArenaNode* node = malloc(sizeof(ArenaNode)+size);
    assert(node && "Ran out of memory");
    node->next = NULL;
    node->len = 0; 
    node->cap = size;
    return node;
}
#define alignup_to(n, t) (((n+(sizeof(t)-1)) / sizeof(t))*sizeof(t))
void* arena_node_alloc_within(ArenaNode* node, size_t size) {
    if(node->cap < node->len+size) return NULL;
    void* at = node->data+node->len;
    node->len += size;
    node->len = alignup_to(node->len, uintptr_t);
    return at;
}

#define MIN_ARENA_SIZE 1024
void* arena_alloc(Arena* arena, size_t size) {
    size_t ncap = size < MIN_ARENA_SIZE ? MIN_ARENA_SIZE : size*2;
    if(!arena->node) {
        arena->node = arena_node_new(ncap);
        return arena_node_alloc_within(arena->node, size);
    }
    ArenaNode* prev = NULL;
    ArenaNode* node = arena->node;
    while(node) {
        void* at = arena_node_alloc_within(node, size);
        if(at) return at;
        prev = node;
        node = node->next;
    }
    prev->next = arena_node_new(ncap);
    return arena_node_alloc_within(prev->next, size);
}
void arena_reset(Arena* arena) {
    ArenaNode* node = arena->node;
    while(node) {
        node->len = 0;
        node = node->next;
    }
}
void arena_drop(Arena* arena) {
    ArenaNode* node = arena->node;
    while(node) {
        ArenaNode* next = node->next;
        free(node);
        node = next;
    }
    arena->node = NULL;
}
const char* dup_str_range(Arena* arena, const char* start, const char* end) {
    size_t len = (size_t)(end-start);
    char* str = arena_alloc(arena, len+1);
    memcpy(str, start, len);
    str[len] = '\0';
    return str;
}
const char* strip_cmd(Arena* arena, const char** str_result) {
    const char* at=*str_result;
    const char* begin=at;
    while(*at) {
        if(isspace(*at)) {
            *str_result = at+1;
            return dup_str_range(arena, begin, at);
        }
        at++;
    }
    *str_result = at;
    return dup_str_range(arena, begin, at);
}
const char* strip_arg(Arena* arena, const char** str_result) {
    const char* at=trim_l(*str_result);
    const char* begin=at;
    if(at[0] == '"') {
        fprintf(stderr, "ERROR: Parsing quoted arguments is not yet supported");
        exit(1);
    }
    while(*at) {
        if(isspace(*at)) {
            *str_result = at+1;
            return dup_str_range(arena, begin, at);
        }
        at++;
    }
    *str_result = at;
    return dup_str_range(arena, begin, at);
}
#define LINEBUF_MAX 1024
#define MAX_ARGS 128
void run_cmd(const char** argv, size_t argc) {
    assert(argc > 0);
    intptr_t e = fork();
    if(e == (-YOU_ARE_CHILD)) {
        if((e=exec(argv[0], argv, argc)) < 0) {
            exit(-e);
        }
        // Unreachable
        exit(0);
    } else if (e >= 0) {
        size_t pid = e;
        e=wait_pid(pid);
        if(e != 0) {
           if(e == NOT_FOUND) {
               printf("Could not find command `%s`\n", argv[0]);
           } else {
               printf("%s exited with: %d\n", argv[0], (int)e);
           }
        }
    } else {
        printf("ERROR: fork %s\n",status_str(e));
        exit(1);
    }
}
// Defined in MinOS libc.
// Otherwise, include it in the current directory (TODO:)
#include "strinternal.h"
int main() {
    printf("Started MinOS shell\n");
    Arena arena={0};
    char* linebuf = malloc(LINEBUF_MAX);
    intptr_t e = 0;
    assert(MAX_ARGS > 0);
    const char** args = malloc(MAX_ARGS*sizeof(*args));
    char* cwd = malloc(PATH_MAX);
    if((e=getcwd(cwd, PATH_MAX)) < 0) {
        fprintf(stderr, "ERROR: Failed to getcwd on initial getcwd: %s\n", status_str(e));
        free(args);
        free(cwd);
        free(linebuf);
        return 1;
    }
    size_t arg_count=0;
    bool running = true;
    int exit_code = 0;

    while(running) {
        printf("%s > ", cwd);
        arena_reset(&arena);
        arg_count=0;
        if((e=readline(linebuf, LINEBUF_MAX-1)) < 0) {
            printf("Failed to read on stdin: %s\n", status_str(e));
            return 1;
        }
        linebuf[e] = 0;
        const char* line = trim_r(linebuf);
        // Empty
        if(line[0] == '\0') continue;
        const char* cmd = strip_cmd(&arena, &line);
        args[arg_count++] = cmd;
        while((line=trim_l(line))[0]) {
            if(arg_count == MAX_ARGS) {
                printf("Forbidden: Maximum argument count reached\n");
                continue;
            }
            const char* arg = strip_arg(&arena, &line);
            args[arg_count++] = arg;
        }
        if(strcmp(cmd, "exit") == 0) {
            running = false;
            if (arg_count == 1) exit_code=0;
            else if (arg_count == 2) {
                const char* end;
                exit_code = atoi_internal(args[1], &end);
                if(end[0] != '\0') {
                    fprintf(stderr, "Invalid exit code `%s`\n", args[1]);
                    exit_code = 1;
                    continue;
                }
            } else {
                fprintf(stderr, "Too many arguments provided to `exit`\n");
                continue;
            }
        } else if (strcmp(cmd, "cd") == 0) {
            if (arg_count < 2) {
                fprintf(stderr, "Expected path after cd\n");
                continue;
            }
            else if (arg_count > 2) {
                fprintf(stderr, "Too many arguments after cd\n");
                continue;
            }
            const char* path = args[1];
            if((e=chdir(path)) < 0) {
                fprintf(stderr, "Failed to cd into `%s`: %s\n", path, status_str(e));
                continue;
            }
            if((e=getcwd(cwd, PATH_MAX)) < 0) {
                fprintf(stderr, "Failed to get cwd: %s\n", status_str(e));
                exit(1); 
            }
        } else {
            run_cmd(args, arg_count);
        }
    }
    arena_drop(&arena);
    free(args);
    return exit_code;
}
