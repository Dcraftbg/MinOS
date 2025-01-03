#pragma once
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
typedef struct {
    ACPISDTHeader header;
} RSDT;
typedef struct {
    ACPISDTHeader header;
} XSDT;
void init_acpi();
ACPISDTHeader* acpi_find(const char* signature);
