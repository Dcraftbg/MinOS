#include <stdio.h>
#include <sys/socket.h>
#include <minos/sysstd.h>
#include <minos/status.h>
#include <string.h>
#include <errno.h>
#include <libwm.h>
#include <libwm/tags.h>
#include <assert.h>
#include <sys/epoll.h>

#define WM_PATH "/sockets/wm"
static int minos_connectto(const char* addr) {
    int fd = socket(AF_MINOS, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("socket");
        return -errno;
    }
    struct sockaddr_minos server_addr;
    server_addr.sminos_family = AF_MINOS;
    strcpy(server_addr.sminos_path, addr);
    if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "connect %s: %s\n", addr, strerror(errno));
        close(fd);
        return -errno;
    }
    return fd;
}
static intptr_t write_exact(int fd, const void* buf, size_t size) {
    intptr_t e;
    while(size) {
        e = write(fd, buf, size);
        if(e < 0) return e;
        if(e == 0) return -PREMATURE_EOF;
        size -= (size_t)e;
        buf += e;
    }
    return 0;
}
static intptr_t write_packet(int fd, uint32_t payload_size, uint16_t tag) {
    intptr_t e;
    payload_size += sizeof(tag); // <- We need to include tag into the payload_size
    if((e=write_exact(fd, &payload_size, sizeof(payload_size))) < 0) return e;
    if((e=write_exact(fd, &tag, sizeof(tag))) < 0) return e;
    return 0;
}
static void sleep_milis(size_t milis) {
    MinOS_Duration duration;
    size_t secs = milis / 1000, rem = milis % 1000;
    duration.secs = secs;
    duration.nano = rem * 1000000;
    sleepfor(&duration);
}
typedef WmEvent PlutoEvent;
typedef struct {
    PlutoEvent* buffer;
    uint32_t head, tail, cap;
} PlutoEventQueue;
typedef struct {
    PlutoEventQueue event_queue;
} PlutoWindow;
typedef struct {
    PlutoWindow** items;
    size_t len, cap;
} PlutoWindows;
typedef struct {
    int client_handle;
    PlutoWindows windows;
} PlutoInstance;
typedef enum { 
    // i.e. check errno for error
    PLUTO_ERR_NETWORKING = 1,
    PLUTO_ERR_CORRUPTED_PACKET,
    PLUTO_ERR_INVALID_PACKET_SIZE,
    PLUTO_ERR_INVALID_PACKET_TAG,
    PLUTO_ERR_PACKET_SIZE_TOO_BIG,
    PLUTO_ERR_INVALID_WINDOW_HANDLE,
    PLUTO_ERR_NOT_ENOUGH_MEMORY,

    PLUTO_ERR_COUNT,
} PlutoError;
#define WM_HEADER_SIZE (sizeof(uint32_t)+sizeof(uint16_t))
size_t _pluto_write_wm_header(char* buf, uint32_t size, uint16_t tag) {
    *(uint32_t*)(buf + 0) = size + sizeof(tag);
    *(uint16_t*)(buf + 4) = tag;
    return WM_HEADER_SIZE;
}
int _pluto_write(int handle, const void* buf, size_t n) {
    intptr_t e = write(handle, buf, n);
    fprintf(stderr, "write!\n");
    if(e != (intptr_t)n) {
        fprintf(stderr, "on write: %s\n", status_str(e));
        errno = e < 0 ? _status_to_errno(e) : EMSGSIZE;
        return -PLUTO_ERR_NETWORKING;
    }
    return 0;
}
int _pluto_read(int handle, void* buf, size_t n) {
    intptr_t e = read(handle, buf, n);
    fprintf(stderr, "read!\n");
    if(e != (intptr_t)n) {
        fprintf(stderr, "on read: %s (%ld; %zu)\n", status_str(e), e, n);
        errno = e < 0 ? _status_to_errno(e) : EBADMSG;
        return -PLUTO_ERR_NETWORKING;
    }
    return 0;
}
// 1  => it was an event
// 0  => it was a response
// <0 => error
int pluto_read_next_packet(PlutoInstance* instance, int32_t* response) {
    uint32_t size;
    int e = _pluto_read(instance->client_handle, &size, sizeof(size));
    if(e < 0) return e;
    if(size < 2) return -PLUTO_ERR_CORRUPTED_PACKET;
    size -= 2;
    uint16_t tag;
    e = _pluto_read(instance->client_handle, &tag, sizeof(tag));
    if(e < 0) return e;
    char buf[32];
    if(sizeof(buf) < size) return -PLUTO_ERR_PACKET_SIZE_TOO_BIG;
    e = _pluto_read(instance->client_handle, buf, size);
    if(e < 0) return e;
    if(tag == WM_PACKET_TAG_RESULT) {
        if(size != 4) return -PLUTO_ERR_INVALID_PACKET_SIZE;
        *response = *(int32_t*)buf;
        return 0;
    } else if (tag == WM_PACKET_TAG_EVENT) {
        if(size < sizeof(WmEvent)) return -PLUTO_ERR_INVALID_PACKET_SIZE;
        WmEvent* event = (WmEvent*)buf;
        if(event->window >= instance->windows.len) return -PLUTO_ERR_INVALID_WINDOW_HANDLE;
        PlutoWindow* window = instance->windows.items[event->window];
        if((window->event_queue.head + 1) % window->event_queue.cap == window->event_queue.tail) 
            return 1;
            // TODO: think about weather to do this or just ignore like we there^
            // return -PLUTO_ERR_EVENT_QUEUE_FULL;
        window->event_queue.head = window->event_queue.head % window->event_queue.cap;
        window->event_queue.buffer[window->event_queue.head++] = *event;
        return 1;
    } 
    return -PLUTO_ERR_INVALID_PACKET_TAG;
}
int pluto_read_response(PlutoInstance* instance, int* response) {
    int e;
    while((e = pluto_read_next_packet(instance, response)) == 1);
    if(e == 0 && *response < 0) {
        fprintf(stderr, "TBD: handle translation from MinOS errors to PlutoError: %s\nIdeally this shit should've been done on the protocol level\n", status_str(*response));
        abort();
    }
    return e;
}
int pluto_create_instance(PlutoInstance* result) {
    memset(result, 0, sizeof(*result));
    result->client_handle = minos_connectto(WM_PATH);
    return result->client_handle < 0 ? -PLUTO_ERR_NETWORKING : 0;
}

int pluto_create_window(PlutoInstance* instance, WmCreateWindowInfo* info, size_t event_queue_cap) {
    int e;
    // Convenient default values
    if(info->title_len == 0 && !info->title) info->title_len = strlen(info->title);
    PlutoWindow* window = malloc(sizeof(*window));
    if(!window) return -PLUTO_ERR_NOT_ENOUGH_MEMORY;
    memset(window, 0, sizeof(*window));
    window->event_queue.buffer = malloc(event_queue_cap * sizeof(*window->event_queue.buffer));
    if(!window->event_queue.buffer) {
        e = -PLUTO_ERR_NOT_ENOUGH_MEMORY;
        goto err_event_queue_alloc;
    }
    window->event_queue.cap = event_queue_cap;
    if(instance->windows.len + 1 > instance->windows.cap) {
        size_t ncap = instance->windows.cap * 2 + 1;
        void* new_windows = realloc(instance->windows.items, ncap * sizeof(*instance->windows.items));
        if(!new_windows) {
            e = -PLUTO_ERR_NOT_ENOUGH_MEMORY;
            goto err_resizing_windows_alloc;
        }
        instance->windows.items = new_windows;
        instance->windows.cap = ncap;
    }

    size_t size = size_WmCreateWindowInfo(info);
    char* buf = malloc(WM_HEADER_SIZE + size);
    if(!buf) {
        e = -PLUTO_ERR_NOT_ENOUGH_MEMORY;
        goto err_alloc_buf;
    }
    size_t n = WM_HEADER_SIZE + write_memory_WmCreateWindowInfo(buf + _pluto_write_wm_header(buf, size, WM_PACKET_TAG_CREATE_WINDOW), info);
    e = _pluto_write(instance->client_handle, buf, n);
    free(buf);
    if(e < 0) goto err_write;
    int response;
    e = pluto_read_response(instance, &response);
    if(e < 0) goto err_read;
    if((size_t)response == instance->windows.len) instance->windows.len++;
    else if((size_t)response > instance->windows.len) {
        e = -PLUTO_ERR_INVALID_WINDOW_HANDLE;
        goto err_invalid_window_handle;
    }
    // TODO: maybe check if window already existed?
    instance->windows.items[(size_t)response] = window;
    return response;
err_invalid_window_handle:
    // TODO: close window here.
err_read:
err_write:
err_alloc_buf:
err_resizing_windows_alloc:
    free(window->event_queue.buffer);
err_event_queue_alloc:
    free(window);
    return e;
}
int pluto_create_shm_region(PlutoInstance* instance, const WmCreateSHMRegion* info) {
    char buf[128];
    size_t size = size_WmCreateSHMRegion(info);
    assert(WM_HEADER_SIZE + size < sizeof(buf));
    int e = _pluto_write(instance->client_handle, buf, WM_HEADER_SIZE + write_memory_WmCreateSHMRegion(buf + _pluto_write_wm_header(buf, size, WM_PACKET_TAG_CREATE_SHM_REGION), info));
    if(e < 0) return e;
    int resp;
    e = pluto_read_response(instance, &resp);
    if(e < 0) return e;
    return resp;
}
int pluto_draw_shm_region(PlutoInstance* instance, const WmDrawSHMRegion* info) {
    char buf[128];
    size_t size = size_WmDrawSHMRegion(info);
    assert(WM_HEADER_SIZE + size < sizeof(buf));
    int e = _pluto_write(instance->client_handle, buf, WM_HEADER_SIZE + write_memory_WmDrawSHMRegion(buf + _pluto_write_wm_header(buf, size, WM_PACKET_TAG_DRAW_SHM_REGION), info));
    if(e < 0) return e;
    int resp;
    e = pluto_read_response(instance, &resp);
    if(e < 0) return e;
    return resp;
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
        .size = 500*500*sizeof(uint32_t)
    });
    uint32_t* addr = NULL;
    assert(_shmmap(shm, (void**)&addr) >= 0);
    int dvd_x = 0, dvd_y = 0;
    int dvd_dx = 1, dvd_dy = 1;
    int dvd_w = 100;
    int dvd_h = 30;
    for(;;) {
        dvd_x += dvd_dx;
        dvd_y += dvd_dy;
        if(dvd_x < 0) {
            dvd_x = 0;
            dvd_dx = -dvd_dx;
        }
        if(dvd_x + dvd_w > (int)width) {
            dvd_x = width-dvd_w;
            dvd_dx = -dvd_dx;
        }
        if(dvd_y < 0) {
            dvd_y = 0;
            dvd_dy = -dvd_dy;
        }
        if(dvd_y + dvd_h > (int)height) {
            dvd_y = height-dvd_h;
            dvd_dy = -dvd_dy;
        }
        for(size_t y = 0; y < height; ++y) {
            for(size_t x = 0; x < width; ++x) {
                addr[y * width + x] = 0xFF111111;
            }
        }
        for(size_t y = dvd_y; y < (size_t)dvd_y + dvd_h; ++y) {
            for(size_t x = dvd_x; x < (size_t)dvd_x + dvd_w; ++x) {
                addr[y * width + x] = 0xFFFF0000;
            }
        }
        pluto_draw_shm_region(&instance, &(WmDrawSHMRegion){
            .window = win,
            .shm_key = shm,
            .width = width,
            .height = height,
            .pitch_bytes = width * sizeof(uint32_t),
        });
    }
}
#if 0
int main2(void) {
    printf("Hello World!\n");
    printf("Connecting...\n");
    int wm = minos_connectto(WM_PATH);
    if(wm < 0) {
        perror("minos_connectto(" WM_PATH ")");
        return 1;
    }
    printf("Connected!\n");
    printf("Creating window!\n");

    WmCreateWindowInfo info = {
        .width = 500,
        .height = 500,
        .title = "Hello bro",
    };
    info.title_len = strlen(info.title);
    int window = create_window(wm, &info);
    assert(window >= 0);
    WmCreateSHMRegion shm_info = {
        .size = info.width * info.height * sizeof(uint32_t)
    };
    int shm = create_shm_region(wm, &shm_info);
    assert(shm >= 0);
    uint32_t* addr = NULL;
    assert(_shmmap(shm, (void**)&addr) >= 0);
    int dvd_x = 0, dvd_y = 0;
    int dvd_dx = 1, dvd_dy = 1;
    int dvd_w = 100;
    int dvd_h = 30;
    int epoll = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    epoll_ctl(epoll, EPOLL_CTL_ADD, wm, &ev);
    for(;;) {
        dvd_x += dvd_dx;
        dvd_y += dvd_dy;
        if(dvd_x < 0) {
            dvd_x = 0;
            dvd_dx = -dvd_dx;
        }
        if(dvd_x + dvd_w > info.width) {
            dvd_x = info.width-dvd_w;
            dvd_dx = -dvd_dx;
        }
        if(dvd_y < 0) {
            dvd_y = 0;
            dvd_dy = -dvd_dy;
        }
        if(dvd_y + dvd_h > info.height) {
            dvd_y = info.height-dvd_h;
            dvd_dy = -dvd_dy;
        }

        for(size_t y = 0; y < info.height; ++y) {
            for(size_t x = 0; x < info.width; ++x) {
                addr[y * info.width + x] = 0xFF111111;
            }
        }
        for(size_t y = dvd_y; y < dvd_y + dvd_h; ++y) {
            for(size_t x = dvd_x; x < dvd_x + dvd_w; ++x) {
                addr[y * info.width + x] = 0xFFFF0000;
            }
        }
        WmDrawSHMRegion draw_info = {
            .window = window,
            .shm_key = shm,
            .width = info.width,
            .height = info.height,
            .pitch_bytes = info.width * sizeof(uint32_t),
        };
        assert(draw_shm_region(wm, &draw_info) >= 0);
        // int n = epoll_wait(epoll, 0, 0, 0);
        // sleep_milis(1000/60);
    }
    close(wm);
    return 0;
}
#endif
