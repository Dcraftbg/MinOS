#include <minos/sysstd.h>
#include <minos/status.h>
#include <stdexec.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <environ.h>
#include <ctype.h>
#include <assert.h>
#include <minos/tty/tty.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

intptr_t readline(char* buf, size_t bufmax) {
    intptr_t e;
    size_t n = 0;
    while(n < bufmax) {
        e = read(STDIN_FILENO, buf + n, bufmax - n);
        if(e < 0) return e;
        assert(e != 0);
        n += e;
        if(buf[n-1] == '\n') break;
    }
    if(n >= bufmax) return -BUFFER_TOO_SMALL;
    return n;
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
typedef struct {
    size_t pid;
    size_t exit_code;
} Cmd;
#define EXEC_STATUS_OFF 1024 
intptr_t spawn_cmd(Cmd* cmd, const char**argv, size_t argc) {
    assert(argc);
    int e = fork();
    if(e == 0) {
        e = execvp(argv[0], argv, argc);
        exit(EXEC_STATUS_OFF + (-e));
    } else if (e >= 0) {
        cmd->pid = e;
    } else return e;
    return 0;
}
intptr_t wait_cmd(Cmd* cmd) {
    intptr_t e = wait_pid(cmd->pid);
    cmd->exit_code = e;
    if(e < 0) return e;
    return e > EXEC_STATUS_OFF ? -(e - EXEC_STATUS_OFF) : e; 
}
void run_cmd(const char** argv, size_t argc) {
    Cmd cmd = { 0 };
    intptr_t e;
    if((e=spawn_cmd(&cmd, argv, argc)) < 0) {
        fprintf(stderr, "ERROR: fork %s\n", status_str(e));
        exit(1);
    }
    if((e=wait_cmd(&cmd)) < 0) {
        fprintf(stderr, "ERROR: Failed to run %s: %s\n", argv[0], status_str(e));
        return;
    }
    if(e != 0) printf("%s exited with: %d\n", argv[0], (int)e);
}
int main() {
    printf("Started MinOS shell\n");
    Arena arena={0};
    char* linebuf = malloc(LINEBUF_MAX);
    intptr_t e = 0;
    assert(MAX_ARGS > 0);
    const char** args = malloc(MAX_ARGS*sizeof(*args));
    char* cwd = malloc(PATH_MAX);
    if(getcwd(cwd, PATH_MAX) == NULL) {
        fprintf(stderr, "ERROR: Failed to getcwd on initial getcwd: %s\n", strerror(errno));
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
        fflush(stdout);
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
                char* end;
                exit_code = strtoll(args[1], &end, 10);
                if(end[0] != '\0') {
                    fprintf(stderr, "Invalid exit code `%s`\n", args[1]);
                    exit_code = 1;
                    continue;
                }
            } else {
                fprintf(stderr, "Too many arguments provided to `exit`\n");
                continue;
            }
        } else if (strcmp(cmd, "clear") == 0) {
            if(arg_count > 1) fprintf(stderr, "Ignoring extra arguments to clear\n");
            printf("\033[2J\033[H");
            fflush(stdout);
        } else if (strcmp(cmd, "reset") == 0) {
            tty_set_flags(fileno(stdin), TTY_ECHO);
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
            if(getcwd(cwd, PATH_MAX) == NULL) {
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
