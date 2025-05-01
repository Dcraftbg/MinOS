#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t _reserved;
    uint64_t rsp0;
    uint32_t _reserved2[23];
} __attribute__((packed)) TSS;
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_flags;
    uint8_t base_high;
    uint32_t base_high2;
    uint32_t _reserved;
} __attribute__((packed)) TSSSegment;
void reload_tss(void);
