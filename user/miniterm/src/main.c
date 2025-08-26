#include <pluto.h>
#include <libwm/events.h>
#include <libwm/key.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <assert.h>
#include <minos/sysstd.h>
#include <minos/status.h>
#include <minos/ptm/ptm.h>
#include <minos/tty/tty.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <sys/epoll.h>
static void free_flanterm(void* ptr, size_t) {
    free(ptr);
}
typedef struct {
    size_t index;
    int master_handle, slave_handle;
} Ptty;
intptr_t ptty_setup(Ptty* ptty) {
    intptr_t e;
    if((e=open("/devices/ptm", O_RDWR)) < 0) {
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
    if((e=open(name, O_RDWR)) < 0) {
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
        (void)ptty;
        char name[120];
        snprintf(name, sizeof(name), "/devices/pts%zu", ptty->index);
        close(STDOUT_FILENO);
        close(STDIN_FILENO);
        close(STDERR_FILENO);
        if(open(name, O_WRONLY) < 0 || /*STDOUT*/
           open(name, O_RDONLY) < 0 || /*STDIN*/
           open(name, O_WRONLY) < 0    /*STDERR*/
        ) {
            return 1;
        }
        // assert(e == STDERR_FILENO);
        const char* argv[] = {
            "shell", NULL
        };
        if(execvp(argv[0], (char*const*)argv) < 0) fprintf(stderr, "ERROR: Failed to spawn shell: %s\n", strerror(errno));
        exit(1);
    } else if(e < 0) {
        fprintf(stderr, "ERROR: Failed to fork: %s\n", strerror(errno));
        return e;
    }
    return e;
}
int epoll_add_fd(int epollfd, int fd, int events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}
int main(void) {
    PlutoInstance instance;
    pluto_create_instance(&instance);
    size_t width = 500, height = 500;
    int win = pluto_create_window(&instance, &(WmCreateWindowInfo) {
        .width = width,
        .height = height,
        .title = "Hello bro"
    }, 16);
    int shm = pluto_create_shm_region(&instance, &(WmCreateSHMRegion) {
        .size = width*height*sizeof(uint32_t)
    });
    uint32_t* addr = NULL;
    assert(_shmmap(shm, (void**)&addr) >= 0);
    for(size_t i = 0; i < width*height; ++i) {
        addr[i] = 0xFF444444;
    }
    // ARGB
    struct flanterm_context* ctx = flanterm_fb_init(
        malloc,
        free_flanterm,
        addr, width, height, width*sizeof(uint32_t),
        8, 16,
        8, 8,
        8, 0,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );
    Ptty ptty = { 0 };
    intptr_t e;
    e = ptty_setup(&ptty);
    assert(e >= 0);
    tty_set_flags(ptty.master_handle, TTY_INSTANT);
    size_t cols, rows;
    flanterm_get_dimensions(ctx, &cols, &rows);
    TtySize tty_size = {
        cols, rows
    };
    tty_set_size(ptty.master_handle, &tty_size);
    e = ptty_spawn_shell(&ptty);
    assert(e >= 0);

    int epoll = epoll_create1(0);
    assert(epoll >= 0);
    assert(epoll_add_fd(epoll, ptty.master_handle, EPOLLIN | EPOLLHUP) >= 0);
    assert(epoll_add_fd(epoll, instance.client_handle, EPOLLIN) >= 0);
    pluto_draw_shm_region(&instance, &(WmDrawSHMRegion){
        .window = win,
        .shm_key = shm,
        .width = width,
        .height = height,
        .pitch_bytes = width * sizeof(uint32_t),
    });
#define MAX_EPOLL_EVENTS 16
    static struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
    for(;;) {
        bool wrote_something = false;
        int n = epoll_wait(epoll, epoll_events, MAX_EPOLL_EVENTS, -1);
        assert(n >= 0);
        for(size_t i = 0; i < (size_t)n; ++i) {
            struct epoll_event* event = &epoll_events[i];
            if(event->events & EPOLLIN) {
                if(event->data.fd == ptty.master_handle) {
                    static char buf[1024];
                    intptr_t e = read(ptty.master_handle, buf, sizeof(buf));
                    assert(e >= 0);
                    if(e > 0) {
                        wrote_something = true;
                        flanterm_write(ctx, buf, (size_t)e);
                    }
                } else if(event->data.fd == instance.client_handle) {
                    int _resp;
                    pluto_read_next_packet(&instance, &_resp);
                    (void)_resp;
                }
            }
            if(event->events & EPOLLHUP) goto end;
        }
        PlutoEventQueue* evq = &instance.windows.items[win]->event_queue;
        while(evq->head != evq->tail) {
            evq->tail = evq->tail % evq->cap;
            size_t at = evq->tail++;
            PlutoEvent event = evq->buffer[at];

            switch(event.event) {
            case WM_EVENT_KEY_DOWN:
                int code = WM_GETKEYCODE(&event);
                if(code) {
                    write(ptty.master_handle, &code, 1);
                }
                break;
            }
        }
        if(wrote_something) {
            pluto_draw_shm_region(&instance, &(WmDrawSHMRegion){
                .window = win,
                .shm_key = shm,
                .width = width,
                .height = height,
                .pitch_bytes = width * sizeof(uint32_t),
            });
        }
    }
end:
    return 0;
}
