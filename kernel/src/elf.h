#pragma once
#define ELF_MAGIC 0x464c457f  // {0x7f, 'E', 'L', 'F' }
typedef struct {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t size/*header*/;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t str_table;
} Elf64Header;
typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
} Elf64Sheader;
typedef struct {
    uint32_t p_type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virt_addr;
    uint64_t phys_addr;
    uint64_t filesize;
    uint64_t memsize;
    uint64_t align;
} Elf64ProgHeader;
#define ELF_DATA_LITTLE_ENDIAN 1
#define ELF_SECTION_PROGBITS 1
#define ELF_SECTION_SYMTABLE 2
#define ELF_SECTION_STRTABLE 3
#define ELF_SECTION_NOTE 7
#define ELF_CLASS_64BIT 2
#define ELF_PROG_WRITE 0b10
#define ELF_TYPE_EXEC 2
#define ELF_PHREADER_LOAD 0b1

#define ELF_DATA_CLASS 4
#define ELF_DATA_ENCODING 5
static inline bool elf_header_verify(Elf64Header* header) {
    return *((uint32_t*)&header->ident) == ELF_MAGIC;
}
