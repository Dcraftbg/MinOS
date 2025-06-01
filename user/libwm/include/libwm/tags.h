#pragma once
// Client packet
enum {
    WM_PACKET_TAG_CREATE_WINDOW=0x01,
    WM_PACKET_TAG_CREATE_SHM_REGION,
    WM_PACKET_TAG_DRAW_SHM_REGION,
    WM_PACKET_TAG_CS_COUNT,
};
// Server packet tags
enum {
    WM_PACKET_TAG_RESULT=0x01,
    WM_PACKET_TAG_SC_COUNT,
};
