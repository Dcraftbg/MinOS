#pragma once
#include <stdint.h>
#include <stddef.h>
#include <libwm.h>
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

int pluto_create_instance(PlutoInstance* result);
int pluto_create_window(PlutoInstance* instance, WmCreateWindowInfo* info, size_t event_queue_cap);
int pluto_create_shm_region(PlutoInstance* instance, const WmCreateSHMRegion* info);
int pluto_draw_shm_region(PlutoInstance* instance, const WmDrawSHMRegion* info);

int pluto_read_next_packet(PlutoInstance* instance, int* response);
int pluto_read_response(PlutoInstance* instance, int* response);
