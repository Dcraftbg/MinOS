#include <stdio.h>
#include <assert.h>
#include <minos/status.h>
#include <minos/sysstd.h>
#include <minos/ptm/ptm.h>
#include <minos/tty/tty.h>
#include <stdexec.h>

#define STUI_IMPLEMENTATION
#include "stui.h"
typedef struct {
    size_t index;
    int master_handle, slave_handle;
} Ptty;
Ptty _ptty;
intptr_t ptty_setup(Ptty* ptty) {
    intptr_t e;
    if((e=open("/devices/ptm", 0, 0)) < 0) {
        fprintf(stderr, "ERROR: Failed to open ptm: %s\n", status_str(e));
        return e;
    }
    assert(e > 0 && "No ptm present");
    size_t ptm = e;
    if((e=ptm_mkptty(ptm)) < 0) {
        fprintf(stderr, "ERROR: Failed to create ptty: %s\n", status_str(e));
        close(ptm);
        return e;
    }
    ptty->index = e;
    close(ptm);
    // Now lets open them :)
    char name[120];
    snprintf(name, sizeof(name), "/devices/ptm/ptty%zu", ptty->index);
    if((e=open(name, MODE_READ | MODE_WRITE, 0)) < 0) {
        fprintf(stderr, "ERROR: Failed to open ptty (%s): %s\n", name, status_str(e));
        close(ptm);
        return e;
    }
    ptty->master_handle = e;
    return 0;
}
intptr_t ptty_spawn_shell(Ptty* ptty) {
    intptr_t e = fork();
    if(e == -YOU_ARE_CHILD) {
        char name[120];
        close(fileno(stderr));
        snprintf(name, sizeof(name), "/devices/pts%zu", ptty->index);
        if((e=open(name, MODE_READ | MODE_WRITE, 0)) < 0) {
            fprintf(stderr, "ERROR: Failed to open pts (%s): %s\n", name, status_str(e));
            return 1;
        }
        // assert(e == STDERR_FILENO);
        const char* argv[] = {
            "shell"
        };
        if(execvp(argv[0], argv, sizeof(argv)/sizeof(*argv)) < 0) {
            fprintf(stderr, "ERROR: Failed to spawn shell: %s\n", status_str(e));
        }
        exit(1);
    } else if(e < 0) {
        fprintf(stderr, "ERROR: Failed to fork: %s\n", status_str(e));
        return e;
    }
    return e;
}
#include <sys/epoll.h>
int epoll_add_fd(int epollfd, int fd, int events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}
static stui_term_flag_t init_flags;
static void cleanup_tty_flags(void) {
    stui_term_set_flags(init_flags);
}
typedef struct {
    size_t offset;
    size_t len;
} Line;
typedef struct {
    Line* items;
    size_t len, cap;
} Lines;
typedef struct {
    char* items;
    size_t len, cap;
} StringBuffer;
#include "darray.h"
static void stui_fill(size_t l, size_t t, size_t r, size_t b, char c) {
    for(size_t y = t; y < b; ++y) {
        for(size_t x = l; x < r; ++x) {
            stui_putchar(x, y, c);
        }
    }
}

int main() {
    init_flags = stui_term_get_flags();
    atexit(cleanup_tty_flags);
    stui_clear();
    size_t width, height;
    stui_term_get_size(&width, &height);
    stui_setsize(width, height);
    stui_refresh();
    assert(ptty_setup(&_ptty) >= 0);
    intptr_t e = ptty_spawn_shell(&_ptty);
    assert(e >= 0);
    e = epoll_create1(0);
    assert(e >= 0);
    uintptr_t epoll = e;
    assert(epoll_add_fd(epoll, fileno(stdin)      , EPOLLIN) >= 0);
    assert(epoll_add_fd(epoll, _ptty.master_handle, EPOLLIN | EPOLLHUP) >= 0);
    #define MAX_EPOLL_EVENTS 120
    static struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
    tty_set_flags(_ptty.master_handle, TTY_INSTANT | TTY_ECHO);
    tty_set_flags(fileno(stdin), TTY_INSTANT);
    Lines lines = { 0 };
    da_push(&lines, ((Line){0, 0}));
    StringBuffer sb = { 0 };
    da_reserve(&sb, 1);
    for(;;) {
        size_t window_x = 1, window_y = 1;
        size_t window_width = width-2, window_height = height-2;
        size_t x, y = window_y;
        size_t lines_start = lines.len < window_height-1 ? 0 : lines.len - (window_height-1);
        stui_fill(window_x, window_y, window_x + window_width, window_y + window_height, ' ');
        for(size_t i = lines_start; i < lines.len && y < window_height; i++) {
            x = window_x;
            Line* line = &lines.items[i];
            for(size_t j = 0; j < line->len && x < window_width; ++j, ++x) {
                stui_putchar(x, y, (sb.items + line->offset)[j]);
            }
            if(i != lines.len-1) y++;
        }
        stui_window_border(0, 0, width-1, height-1, '-', '|', '+');
        stui_refresh();
        stui_goto(x, y);
        int n = epoll_wait(epoll, epoll_events, MAX_EPOLL_EVENTS, -1);
        assert(n >= 0);
        static char buf_window[1024];
        for(size_t i = 0; i < (size_t)n; ++i) {
            struct epoll_event* event = &epoll_events[i];
            if(event->events & EPOLLIN) {
                if(event->data.fd == _ptty.master_handle) {
                    e = read(_ptty.master_handle, buf_window, sizeof(buf_window));
                    if(e < 0) {
                        fprintf(stderr, "ERROR: Failed to read: %s\n", status_str(e));
                        return 1;
                    }
                    for(size_t i = 0; i < (size_t)e; ++i) {
                        char c = buf_window[i];
                        switch(c) {
                        case -1:
                            goto end;
                        case '\n': {
                            da_push(&lines, ((Line){sb.len, 0}));
                        } break;
                        case '\b':
                            if(lines.items[lines.len-1].len > 0) {
                                lines.items[lines.len-1].len--;
                                sb.len--;
                                stui_putchar(x-1, y, ' ');
                            }
                            // if(x > 1) {
                            //     stui_putchar(--x, y, ' ');
                            // }
                            break;
                        default: {
                            if(x >= width-1) {
                                x = 1;
                                y++;
                            }
                            da_push(&sb, c);
                            lines.items[lines.len-1].len++;
                        }
                            // stui_putchar(x++, y, c);
                        }
                    }
                } else if (event->data.fd == fileno(stdin)) {
                    e = read(fileno(stdin), buf_window, sizeof(buf_window));
                    if(e < 0) {
                        fprintf(stderr, "ERROR: Failed to read: %s\n", status_str(e));
                        return 1;
                    }
                    write(_ptty.master_handle, buf_window, e);
                }
            } else if(event->events & EPOLLHUP) {
                stui_clear();
                goto end;
            }
        }
    }
end:
    return 0;
}
