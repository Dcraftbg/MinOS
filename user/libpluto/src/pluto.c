#include <pluto.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <libwm/tags.h>
#include <string.h>
#include <stdio.h>
#include <minos/sysstd.h>
#include <minos/status.h>

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
#define WM_HEADER_SIZE (sizeof(uint32_t)+sizeof(uint16_t))
static size_t _pluto_write_wm_header(char* buf, uint32_t size, uint16_t tag) {
    *(uint32_t*)(buf + 0) = size + sizeof(tag);
    *(uint16_t*)(buf + 4) = tag;
    return WM_HEADER_SIZE;
}
static int _pluto_write(int handle, const void* buf, size_t n) {
    intptr_t e = write(handle, buf, n);
    if(e != (intptr_t)n) {
        errno = e < 0 ? _status_to_errno(e) : EMSGSIZE;
        return -PLUTO_ERR_NETWORKING;
    }
    return 0;
}
static int _pluto_read(int handle, void* buf, size_t n) {
    intptr_t e = read(handle, buf, n);
    if(e != (intptr_t)n) {
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
