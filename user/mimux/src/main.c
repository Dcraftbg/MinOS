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
intptr_t ptty_setup(Ptty* ptty, const TtySize* size) {
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
    assert(tty_set_size(ptty->master_handle, size) >= 0);
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
typedef struct {
    size_t x, y;
    size_t width, height;
} Region;
static void render_region_border(Region region, int tb, int lr, int corner) {
    stui_window_border(region.x, region.y, region.width-1, region.height-1, tb, lr, corner);
}
static Region region_chop_vert(Region* region, size_t height) {
    Region result = {
        region->x, region->y,
        region->width, height
    };
    region->y += height;
    region->height -= height;
    return result;
}
static Region region_chop_horiz(Region* region, size_t width) {
    Region result = {
        region->x, region->y,
        width, region->height
    };
    region->x += width;
    region->width -= width;
    return result;
}
typedef struct {
    Region region;
    StringBuffer sb;
    Lines lines;
    Ptty ptty;
    size_t cursor_x, cursor_y;
} Window;
void draw_window(Window* window) {
    size_t x, y = window->region.y + 1;
    size_t lines_start = window->lines.len < window->region.height-2 ? 0 : window->lines.len - (window->region.height-2);
    stui_fill(window->region.x + 1, window->region.y + 1, window->region.x + window->region.width-1, window->region.y + window->region.height-1, ' ');
    for(size_t i = lines_start; i < window->lines.len && y < window->region.y + window->region.height-1; i++) {
        x = window->region.x + 1;
        Line* line = &window->lines.items[i];
        for(size_t j = 0; j < line->len && x < window->region.x + window->region.width-1; ++j, ++x) {
            stui_putchar(x, y, (window->sb.items + line->offset)[j]);
        }
        if(i != window->lines.len-1) y++;
    }
    window->cursor_x = x;
    window->cursor_y = y;
    // stui_window_border(window->region.x, window->region.y, window->region.width-1, window->region.height-1, '-', '|', '+');
    render_region_border(window->region, '-', '|', '+');
}
void window_putchar(Window* window, int c) {
    switch(c) {
    case '\n': {
        da_push(&window->lines, ((Line){window->sb.len, 0}));
    } break;
    case '\b':
        if(window->lines.items[window->lines.len-1].len > 0) {
            window->lines.items[window->lines.len-1].len--;
            window->sb.len--;
            stui_putchar(window->cursor_x-1, window->cursor_y, ' ');
        }
        break;
    default: {
        if(window->cursor_x >= window->region.x + window->region.width-1) {
            window->cursor_x = window->region.x + 1;
            window->cursor_y++;
        }
        da_push(&window->sb, c);
        window->lines.items[window->lines.len-1].len++;
    }
    }
}
typedef struct {
    Window* items;
    size_t len, cap;
} Windows;
int main(void) {
    init_flags = stui_term_get_flags();
    atexit(cleanup_tty_flags);
    stui_clear();
    size_t width, height;
    stui_term_get_size(&width, &height);
    height--; // <- A temporary fix
    stui_setsize(width, height);
    stui_refresh();

    Windows windows = { 0 };
    size_t selected = 0;
    da_push(&windows, (Window){0});
    TtySize size = { windows.items[windows.len-1].region.width, windows.items[windows.len-1].region.height };
    assert(ptty_setup(&windows.items[windows.len-1].ptty, &size) >= 0);
    intptr_t e = ptty_spawn_shell(&windows.items[windows.len-1].ptty);
    assert(e >= 0);
    e = epoll_create1(0);
    assert(e >= 0);
    uintptr_t epoll = e;
    assert(epoll_add_fd(epoll, fileno(stdin), EPOLLIN) >= 0);
    assert(epoll_add_fd(epoll, windows.items[windows.len-1].ptty.master_handle, EPOLLIN | EPOLLHUP) >= 0);
    #define MAX_EPOLL_EVENTS 120
    static struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
    tty_set_flags(windows.items[windows.len-1].ptty.master_handle, TTY_INSTANT | TTY_ECHO);
    tty_set_flags(fileno(stdin), TTY_INSTANT);
    da_push(&windows.items[windows.len-1].lines, ((Line){0, 0}));
    da_reserve(&windows.items[windows.len-1].sb, 1);
    for(;;) {
        Region screen = {
            0, 0,
            width, height
        };
        const size_t window_width = screen.width/windows.len;
        for(size_t i = 0; i < windows.len; ++i) {
            windows.items[i].region = region_chop_horiz(&screen, window_width);
            draw_window(&windows.items[i]);
        }
        stui_refresh();
        stui_goto(windows.items[selected].cursor_x, windows.items[selected].cursor_y);
        int n = epoll_wait(epoll, epoll_events, MAX_EPOLL_EVENTS, -1);
        assert(n >= 0);
        static char buf_window[1024];
        for(size_t i = 0; i < (size_t)n; ++i) {
            struct epoll_event* event = &epoll_events[i];
            if(event->events & EPOLLIN) {
                if (event->data.fd == fileno(stdin)) {
                    e = read(fileno(stdin), buf_window, sizeof(buf_window));
                    if(e < 0) {
                        fprintf(stderr, "ERROR: Failed to read: %s\n", status_str(e));
                        return 1;
                    }
                    write(windows.items[selected].ptty.master_handle, buf_window, e);
                    continue;
                }
                for(size_t i = 0; i < windows.len; ++i) {
                    if(event->data.fd == windows.items[i].ptty.master_handle) {
                        e = read(windows.items[i].ptty.master_handle, buf_window, sizeof(buf_window));
                        if(e < 0) {
                            fprintf(stderr, "ERROR: Failed to read: %s\n", status_str(e));
                            return 1;
                        }
                        for(size_t i = 0; i < (size_t)e; ++i) {
                            window_putchar(&windows.items[i], buf_window[i]);
                        }
                    }
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
