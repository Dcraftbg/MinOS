#pragma once
#include "utils.h"
#include <stdint.h>
typedef struct RSDP RSDP;
struct RSDP {
    char signature[8];
    uint8_t checksum;
    char OEMID[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

typedef struct XSDP XSDP;
struct XSDP {
    char signature[8];
    uint8_t checksum;
    char OEMID[6];
    uint8_t revision;
    uint32_t rsdt_address;
    char __xsdp_2[0];
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ex_check_sum;
    uint8_t reserved[3];
} __attribute__((packed));

typedef struct ACPISDTHeader ACPISDTHeader;
struct ACPISDTHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char OEMID[6];
    char OEMTABLEID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed));
static_assert(sizeof(ACPISDTHeader) == 36, "ACPISDTHeader alignment is weird");
typedef struct {
    ACPISDTHeader header;
} __attribute__((packed)) RSDT;
static_assert(sizeof(RSDT) == 9*4, "RSDT Misalignment");
typedef struct {
    ACPISDTHeader header;
} __attribute__((packed)) XSDT;
static_assert(sizeof(XSDT) == 9*4, "XSDT Misalignment");
void init_acpi();
ACPISDTHeader* acpi_find(const char* signature);
