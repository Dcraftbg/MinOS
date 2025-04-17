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
#define ELF_ST_BIND(i) ((i) >> 4)
enum {
    ELF_STB_LOCAL,
    ELF_STB_GLOBAL,
    ELF_STB_WEAK,
};
typedef struct {
    uint64_t offset;
    uint64_t info;
     int64_t addend;
} Elf64Rela;
typedef struct {
    uint32_t name;
    uint8_t  info;
    uint8_t  other;
    uint16_t section_index;
    uint64_t value;
    uint64_t size;
} Elf64Sym;
typedef struct {
    uint64_t tag;
    uint64_t ptr;
} Elf64Dynamic;
#define ELF_DYNAMIC_TAG_NULL     0
#define ELF_DYNAMIC_TAG_NEEDED   1
#define ELF_DYNAMIC_TAG_PLTRELSZ 2
#define ELF_DYNAMIC_TAG_HASH     4
#define ELF_DYNAMIC_TAG_STRTAB   5
#define ELF_DYNAMIC_TAG_SYMTAB   6
#define ELF_DYNAMIC_TAG_RELA     7
#define ELF_DYNAMIC_TAG_RELASZ   8
#define ELF_DYNAMIC_TAG_RELAENT  9
#define ELF_DYNAMIC_TAG_REL      17
#define ELF_DYNAMIC_TAG_RELSZ    18
#define ELF_DYNAMIC_TAG_RELENT   19
#define ELF_DYNAMIC_TAG_PLTREL   20
#define ELF_DYNAMIC_TAG_JMPREL   23
#define ELF_DYNAMIC_TAG_COUNT    24


#define ELF_DATA_LITTLE_ENDIAN 1
#define ELF_SECTION_PROGBITS 1
#define ELF_SECTION_SYMTABLE 2
#define ELF_SECTION_STRTABLE 3
#define ELF_SECTION_RELA     4
#define ELF_SECTION_DYNAMIC  6
#define ELF_SECTION_REL      9

#define ELF_SECTION_NOTE 7
#define ELF_CLASS_64BIT 2

#define ELF_PROG_EXEC  0b1
#define ELF_PROG_WRITE 0b10

#define ELF_TYPE_EXEC    2
// P.s. Position Independent.
// The name is stupid
#define ELF_TYPE_DYNAMIC 3

#define ELF_PHEADER_LOAD    1
#define ELF_PHEADER_DYNAMIC 2

#define ELF_DATA_CLASS 4
#define ELF_DATA_ENCODING 5
static inline bool elf_header_verify(Elf64Header* header) {
    return *((uint32_t*)&header->ident) == ELF_MAGIC;
}

#define ELF64_RELA_SYM(i)  ((i) >> 32)
#define ELF64_RELA_TYPE(i) ((uint32_t)i)
#define ELF64_R_TYPE_COPY  5

typedef enum {
    RELOC_X86_64_NONE,
    RELOC_X86_64_64,
    RELOC_X86_64_PC32,
    RELOC_X86_64_GOT32,
    RELOC_X86_64_PLT32,
    RELOC_X86_64_COPY,
    RELOC_X86_64_GLOB_DAT,
    RELOC_X86_64_JUMP_SLOT,
    RELOC_X86_64_RELATIVE,
    RELOC_X86_64_GOTPCREL,
    RELOC_X86_64_32,
    RELOC_X86_64_32S,
    RELOC_X86_64_16,
    RELOC_X86_64_PC16,
    RELOC_X86_64_8,
    RELOC_X86_64_PC8,
    RELOC_X86_64_PC64,
    RELOC_X86_64_GOTOFF64,
    RELOC_X86_64_GOTPC32,
    RELOC_X86_64_SIZE32,
    RELOC_X86_64_SIZE64,
} x86_64_reloc_type;
