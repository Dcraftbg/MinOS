#include <stdint.h>
#include <stdbool.h>
#include <minos/mmap.h>
#include <minos/sysstd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "printf.h"
#include "string.h"


intptr_t _mmap(void** addr, size_t length, int prot, int flags, int fd, off_t offset);
#define __STRINGIFY2(x) #x
#define __STRINGIFY(x) __STRINGIFY2(x)
#define exit _exit
#define assert(x) \
    x ? 0 : (dprintf(STDERR_FILENO, "%s", __FILE__ ":" __STRINGIFY(__LINE__) " Assertion failed: " __STRINGIFY(x) "\n"), exit(1))

#define abort() _exit(1)

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))
#define eprintf(...) dprintf(STDERR_FILENO, __VA_ARGS__)
#define eprintfln(...) (eprintf(__VA_ARGS__), eprintf("\n"))
#define STRINGIFY0(x) # x
#define STRINGIFY1(x) STRINGIFY0(x)
#define unreachable(...) (eprintfln("unreachable:" __FILE__ ":" STRINGIFY1(__LINE__) ": " __VA_ARGS__), abort())
#define unsupported(...) (eprintfln("unsupported:" __FILE__ ":" STRINGIFY1(__LINE__) ": "__VA_ARGS__), abort())
#if 0
# define dbprintf   eprintf
# define dbprintfln eprintfln
#else
# define dbprintf(...)   
# define dbprintfln(...)
#endif

// NOTE: accounting for some leeway between the stack and the dynamic libraries (around 8 pages)
// just to be sure
#define DYNLIB_BASE_ADDRESS 0x000070000000E000LL
uintptr_t _dynlib_base_addr = DYNLIB_BASE_ADDRESS;

#define PAGE_SIZE 4096
#define PAGE_ALIGN_UP(n)   (((n) + (PAGE_SIZE-1))/PAGE_SIZE*PAGE_SIZE)
#define PAGE_ALIGN_DOWN(n) ((n)/PAGE_SIZE*PAGE_SIZE)
uintptr_t dynlib_alloc_addr(size_t bytes) {
    uintptr_t result = _dynlib_base_addr;
    bytes = PAGE_ALIGN_UP(bytes);
    _dynlib_base_addr += bytes;
    return result;
}

#include "elf.h"
const char* elf_type_to_str(uint16_t type) {
    switch(type) {
    case ELF_TYPE_EXEC:
        return "Executable";
    case ELF_TYPE_DYNAMIC:
        return "Dynamic";
    default:
        return "Unknown";
    }
}
const char* shtype_to_str(uint32_t type) {
    switch(type) {
    case ELF_SECTION_PROGBITS:
        return "Progbits";
    case ELF_SECTION_SYMTABLE:
        return "Symbol Table";
    case ELF_SECTION_STRTABLE:
        return "String Table";
    case ELF_SECTION_RELA:
        return "RELA";
    case ELF_SECTION_DYNAMIC:
        return "Dynamic";
    case ELF_SECTION_REL:
        return "REL";
    default:
        return "Unknown";
    }
}
const char* reloc_type_to_str(uint32_t type) {
    switch(type) {
    case RELOC_X86_64_GLOB_DAT:
        return "Glob dat";
    case RELOC_X86_64_RELATIVE:
        return "Relative";
    default:
        return NULL;
    }
}
#define ELF_MAX_DEPS 4
typedef struct Elf Elf;
struct Elf {
    Elf64Header header;
    uintptr_t base;
    uintptr_t dynamics[ELF_DYNAMIC_TAG_COUNT];
    char path[1024];
    size_t deps_count;
    Elf* deps[ELF_MAX_DEPS];
};
#define ELF_CACHE_MAX 10
Elf elf_cache[ELF_CACHE_MAX] = { 0 };
size_t elf_cache_count = 0;
enum {
    ELF_ERROR_MAX_CACHED=1,
    ELF_ERROR_MAX_DEPS,
    ELF_ERROR_IO,
    ELF_ERROR_MAGIC,
};
// intptr_t readat(uintptr_t fd, void* data, size_t n, size_t offset) {
//     assert(lseek(fd, offset, SEEK_SET) != (off_t)-1);
//     assert(read(fd, data, n) == n);
//     return 0;
// }
intptr_t read_exact(uintptr_t fd, void* buf, size_t n) {
    ssize_t e = read(fd, buf, n);
    if(e < 0) {
        eprintfln("ERROR: Io error");
        return -ELF_ERROR_IO;
    }
    if(((size_t)e) != n) {
        eprintfln("ERROR: Io error. Incomplete read");
        return -ELF_ERROR_IO;
    }
    return 0;
}
intptr_t seek_to(uintptr_t fd, size_t n) {
    if(lseek(fd, n, SEEK_SET) == (off_t)-1) {
        eprintfln("ERROR: seek error");
        return -ELF_ERROR_IO;
    }
    return 0;
}

intptr_t load_elf(Elf** result, const char* path, uintptr_t fd);
intptr_t handle_dynamics(Elf* elf, uintptr_t fd, size_t offset, size_t size) {
    intptr_t e;
    if((e=seek_to(fd, offset)) < 0) return e;
    size_t elements = size / sizeof(Elf64Dynamic);
    for(size_t i = 0; i < elements; ++i) {
        Elf64Dynamic dynamic;
        if((e=read_exact(fd, &dynamic, sizeof(dynamic))) < 0) return e;
        if(dynamic.tag == ELF_DYNAMIC_TAG_NULL) break;
        if(dynamic.tag >= ELF_DYNAMIC_TAG_COUNT) continue;
        elf->dynamics[dynamic.tag] = dynamic.ptr;
        switch(dynamic.tag) {
        case ELF_DYNAMIC_TAG_HASH:
        case ELF_DYNAMIC_TAG_STRTAB:
        case ELF_DYNAMIC_TAG_SYMTAB:
        case ELF_DYNAMIC_TAG_REL:
        case ELF_DYNAMIC_TAG_RELA:
        case ELF_DYNAMIC_TAG_JMPREL:
            elf->dynamics[dynamic.tag] += elf->base;
            break;
        }
    }
    if((e=seek_to(fd, offset)) < 0) return e;
    for(size_t i = 0; i < elements; ++i) {
        Elf64Dynamic dynamic;
        if((e=read_exact(fd, &dynamic, sizeof(dynamic))) < 0) return e;
        if(dynamic.tag == ELF_DYNAMIC_TAG_NULL) break;
        if(dynamic.tag != ELF_DYNAMIC_TAG_NEEDED) continue;
        assert(elf->dynamics[ELF_DYNAMIC_TAG_STRTAB]);
        if(elf->deps_count == ELF_MAX_DEPS) return -ELF_ERROR_MAX_DEPS;
        const char* prefix = 
        #if 0
            "/usr/lib/x86_64-linux-gnu/"
        #else   
            "/lib/"
        #endif
        ;
        // TODO: safe vesrion of string functions maybe
        char pathbuf[1024];
        strcpy(pathbuf, prefix);
        strcat(pathbuf, (const char*)(elf->dynamics[ELF_DYNAMIC_TAG_STRTAB]+dynamic.ptr));
        intptr_t needed_fd = open(pathbuf, O_RDONLY);
        if(needed_fd < 0) {
            dbprintfln("Before prefix: %s", (const char*)(elf->dynamics[ELF_DYNAMIC_TAG_STRTAB]+dynamic.ptr));
            dbprintfln("Failed to load dependency: %s", pathbuf);
            return -ELF_ERROR_IO;
        }
        if((e=load_elf(&elf->deps[elf->deps_count++], pathbuf, needed_fd)) < 0) return e;
    }
    return 0;
}
intptr_t load_elf(Elf** result, const char* path, uintptr_t fd) {
    dbprintfln("load_elf(%s)", path);
    if(elf_cache_count == ELF_CACHE_MAX) return -ELF_ERROR_MAX_CACHED;
    for(size_t i = 0; i < elf_cache_count; ++i) {
        if(strcmp(elf_cache[i].path, path) == 0) {
            *result = &elf_cache[i];
            return 0;
        }
    }
    intptr_t e = 0;
    uintptr_t base = 0;
    Elf64Header header;
    if((e=read_exact(fd, &header, sizeof(header))) < 0) goto defer;
    if(!elf_header_verify(&header)) {
        e=-ELF_ERROR_MAGIC;
        goto defer;
    }
    if(header.type == ELF_TYPE_DYNAMIC) {
        uintptr_t eoe = 0;
        if((e=seek_to(fd, header.phoff)) < 0) goto defer;
        for(size_t i = 0; i < header.phnum; ++i) {
            Elf64ProgHeader pheader;
            if((e=read_exact(fd, &pheader, sizeof(Elf64ProgHeader))) < 0) goto defer;
            uintptr_t end = PAGE_ALIGN_UP(pheader.virt_addr + pheader.memsize);
            if(end > eoe) eoe = end;
        }
        base = dynlib_alloc_addr(eoe);
        dbprintfln("Picked position %p", (void*)base);
    }
    Elf64ProgHeader dynamic_header = { 0 };
    for(size_t i = 0; i < header.phnum; ++i) {
        Elf64ProgHeader pheader;
        if((e=seek_to(fd, header.phoff + sizeof(Elf64ProgHeader)*i)) < 0) goto defer;
        if((e=read_exact(fd, &pheader, sizeof(Elf64ProgHeader))) < 0) goto defer;
        switch(pheader.p_type) {
        case ELF_PHEADER_LOAD: {
            if(pheader.memsize == 0) continue;
            pheader.virt_addr += base;
            uintptr_t segment_pages  = (PAGE_ALIGN_UP(pheader.virt_addr + pheader.memsize) - PAGE_ALIGN_DOWN(pheader.virt_addr)) / PAGE_SIZE;
            uintptr_t virt = PAGE_ALIGN_DOWN(pheader.virt_addr);
            uintptr_t virt_off = pheader.virt_addr - virt;
            e = _mmap((void**)&virt, segment_pages * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, 0, 0);
            if(e < 0) {
                eprintfln("ERROR: Failed to mmap at %p (%zu bytes) = %ld", (void*)virt, segment_pages*PAGE_SIZE, e);
                e=-ELF_ERROR_IO;
                goto defer;
            }
            memset((void*)virt, 0, segment_pages*PAGE_SIZE);
            if((e=seek_to(fd, pheader.offset)) < 0) goto defer;
            if((
                e=read_exact(fd, (void*)(virt + virt_off), pheader.filesize)
               ) < 0) goto defer;
            dbprintfln("Section %p->%p (Actually loaded %p->%p)", (void*)virt, (void*)(virt+segment_pages*PAGE_SIZE), (void*)(virt+virt_off), (void*)(virt+virt_off+pheader.filesize));
        } break;
        case ELF_PHEADER_DYNAMIC:
            memcpy(&dynamic_header, &pheader, sizeof(pheader));
            break;
        }
    }
    Elf* elf = &elf_cache[elf_cache_count++];
    memcpy(&elf->header, &header, sizeof(header));
    strcpy(elf->path, path);
    elf->base = base;
    if(dynamic_header.p_type == ELF_PHEADER_DYNAMIC) {
        dbprintfln("TRACE: Contains dynamic!");
        if((e=handle_dynamics(elf, fd, dynamic_header.offset, dynamic_header.filesize)) < 0) goto defer;
    }
    *result = elf;
defer:
    close(fd);
    return e;
}

static uint32_t elf_hash(const char* name)
{
	uint32_t h = 0, g;
	while (*name)
	{
		h = (h << 4) + *name++;
		if ((g = h & 0xF0000000))
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}
Elf64Sym* lookup_symbol(Elf* elf, const char* name) {
    if(!elf->dynamics[ELF_DYNAMIC_TAG_HASH]) {
        eprintfln("File `%s` does not have a hash table", elf->path);
        return NULL;
    }
    assert(elf->dynamics[ELF_DYNAMIC_TAG_HASH]);
    assert(elf->dynamics[ELF_DYNAMIC_TAG_SYMTAB]);
    uint32_t* ht = (uint32_t*)elf->dynamics[ELF_DYNAMIC_TAG_HASH];
    uint32_t n = ht[0];
    for(uint32_t it = ht[2 + (elf_hash(name) % n)]; it; it = ht[2 + n + it]) {
        Elf64Sym* symbol = (Elf64Sym*)(elf->dynamics[ELF_DYNAMIC_TAG_SYMTAB] + it * sizeof(Elf64Sym));
        if(!symbol->section_index) continue;
        const char* symbol_name = (const char*)(elf->dynamics[ELF_DYNAMIC_TAG_STRTAB] + symbol->name);
        if(strcmp(name, symbol_name) == 0) return symbol;
    }
    return NULL;
}
// ----------
void do_copy_relocation(Elf* elf, Elf64Rela* reloc) {
    if(ELF64_RELA_TYPE(reloc->info) != ELF64_R_TYPE_COPY) return;
    uint32_t symbol_index = ELF64_RELA_SYM(reloc->info);
    if(!symbol_index) {
        dbprintfln("WARN: Skipping copy relocation with no symbol_index");
        return;
    }
    // assumes there's a symtab and strtab
    Elf64Sym* symbol = &((Elf64Sym*)elf->dynamics[ELF_DYNAMIC_TAG_SYMTAB])[symbol_index];
    if(!symbol->name) {
        dbprintfln("WARN: Skipping copy relocation for some symbol...");
        return;
    }
    const char* symbol_name = (const char*)(elf->dynamics[ELF_DYNAMIC_TAG_STRTAB] + symbol->name);
    for(size_t i = 0; i < elf_cache_count; ++i) {
        Elf* cached_elf = &elf_cache[i];
        if(cached_elf == elf) continue;
        Elf64Sym* cached_symbol = lookup_symbol(cached_elf, symbol_name);
        if(!cached_symbol) continue;
        dbprintfln("TRACE: copy relocate `%s`", symbol_name);
        memcpy((void*)(elf->base + reloc->offset), (void*)(cached_elf->base + cached_symbol->value), cached_symbol->size);
        return;
    }
    eprintfln("Could not copy relocate: `%s`. Not found", symbol_name);
    exit(1);
}
static void relocate(Elf* elf, Elf64Rela* reloc, uintptr_t addr) {
    size_t size = 0;
    uintptr_t value = 0;
    switch(ELF64_RELA_TYPE(reloc->info)) {
    case RELOC_X86_64_NONE:
        break;
    case RELOC_X86_64_64:
        size = sizeof(uint64_t);
        value = addr + reloc->addend;
        break;
    case RELOC_X86_64_GLOB_DAT:
    case RELOC_X86_64_JUMP_SLOT:
        size = sizeof(uint64_t);
        value = addr;
        break;
    case RELOC_X86_64_RELATIVE:
        size = sizeof(uint64_t);
        value = elf->base + reloc->addend;
        break;
    default:
        unsupported("reloc->type = %zu (`%s`)", (size_t)ELF64_RELA_TYPE(reloc->info), elf->path);
    }
    switch(size) {
    case 0: break;
    case sizeof(uint64_t):
        *(uint64_t*)(elf->base + reloc->offset) = value;
        break;
    default:
        unreachable("size = %zu (`%s`)", size, elf->path);
    }
}
void do_relocation(Elf* elf, Elf64Rela* reloc) {
    if(ELF64_RELA_TYPE(reloc->info) == ELF64_R_TYPE_COPY) return;
    uint32_t symbol_index = ELF64_RELA_SYM(reloc->info);
    if(!symbol_index) {
        dbprintfln("TRACE: relocate anonymous symbol. type=%d offset=%p addend=%p (value=0)", ELF64_RELA_TYPE(reloc->info), (void*)reloc->offset, (void*)reloc->addend);
        relocate(elf, reloc, 0);
        return;
    }
    // assumes there's a symtab and strtab
    Elf64Sym* symbol = &((Elf64Sym*)elf->dynamics[ELF_DYNAMIC_TAG_SYMTAB])[symbol_index];
    const char* symbol_name = (const char*)(elf->dynamics[ELF_DYNAMIC_TAG_STRTAB] + symbol->name);
    uintptr_t   symbol_addr = 0;
    if(ELF_ST_BIND(symbol->info) == ELF_STB_WEAK) {
        symbol_addr = elf->base + symbol->value;
        dbprintfln("TRACE: weak relocate `%s`", symbol_name);
        relocate(elf, reloc, symbol_addr);
        return;
    }
    if(!symbol->name) {
        dbprintfln("WARN: Skipping relocation with no name");
        return;
    }
    for(size_t i = 0; i < elf_cache_count; ++i) {
        Elf* cached_elf = &elf_cache[i];
        // if(cached_elf == elf) continue;
        Elf64Sym* cached_symbol = lookup_symbol(cached_elf, symbol_name);
        if(!cached_symbol) continue;
        if(ELF_ST_BIND(cached_symbol->info) == ELF_STB_WEAK) continue;
        dbprintfln("TRACE: relocate `%s` found inside `%s` bind=%d reloc->type=%d", symbol_name, cached_elf->path, ELF_ST_BIND(cached_symbol->info), ELF64_RELA_TYPE(reloc->info));
        relocate(elf, reloc, cached_elf->base + cached_symbol->value);
        return;
    }
    eprintfln("Could not relocate: `%s`. Not found", symbol_name);
    exit(1);
}
intptr_t relocate_elf(Elf* elf) {
    if(!elf->dynamics[ELF_DYNAMIC_TAG_SYMTAB]) return 0;
    assert(elf->dynamics[ELF_DYNAMIC_TAG_SYMTAB]);
    intptr_t e;
    dbprintfln("TRACE: relocate_elf(%s)", elf->path);
    assert(elf->dynamics[ELF_DYNAMIC_TAG_REL] == 0 && "TBD: REL");
    if(elf->dynamics[ELF_DYNAMIC_TAG_RELA] && elf->dynamics[ELF_DYNAMIC_TAG_RELAENT]) {
        assert(elf->dynamics[ELF_DYNAMIC_TAG_RELASZ]);
        assert(elf->dynamics[ELF_DYNAMIC_TAG_RELAENT]);
        Elf64Rela* rela = (void*)elf->dynamics[ELF_DYNAMIC_TAG_RELA];
        size_t n = elf->dynamics[ELF_DYNAMIC_TAG_RELASZ] / elf->dynamics[ELF_DYNAMIC_TAG_RELAENT];
        for(size_t i = 0; i < n; ++i) {
            do_copy_relocation(elf, rela);
            rela = (Elf64Rela*)(((char*)rela) + elf->dynamics[ELF_DYNAMIC_TAG_RELAENT]);
        }
    }
    for(size_t i = 0; i < elf->deps_count; ++i) {
        if((e=relocate_elf(elf->deps[i])) < 0) return e;
    }
    if(elf->dynamics[ELF_DYNAMIC_TAG_RELA] && elf->dynamics[ELF_DYNAMIC_TAG_RELAENT]) {
        assert(elf->dynamics[ELF_DYNAMIC_TAG_RELASZ]);
        assert(elf->dynamics[ELF_DYNAMIC_TAG_RELAENT]);
        Elf64Rela* rela = (void*)elf->dynamics[ELF_DYNAMIC_TAG_RELA];
        size_t n = elf->dynamics[ELF_DYNAMIC_TAG_RELASZ] / elf->dynamics[ELF_DYNAMIC_TAG_RELAENT];
        for(size_t i = 0; i < n; ++i) {
            do_relocation(elf, rela);
            rela = (Elf64Rela*)(((char*)rela) + elf->dynamics[ELF_DYNAMIC_TAG_RELAENT]);
        }
    }
    if(elf->dynamics[ELF_DYNAMIC_TAG_JMPREL] && elf->dynamics[ELF_DYNAMIC_TAG_PLTRELSZ]) {
        assert(elf->dynamics[ELF_DYNAMIC_TAG_PLTREL] == ELF_DYNAMIC_TAG_RELA);
        size_t n = elf->dynamics[ELF_DYNAMIC_TAG_PLTRELSZ] / sizeof(Elf64Rela);
        for(size_t i = 0; i < n; ++i) {
            do_relocation(elf, &((Elf64Rela*)elf->dynamics[ELF_DYNAMIC_TAG_JMPREL])[i]);
        }
    }
    return 0;
}
const char* shift_args(int *argc, const char ***argv) {
    if((*argc) <= 0) return NULL;
    return ((*argc)--, *((*argv)++));
}
typedef int (*_start_t)(int argc, const char** argv, const char** envv);
int main(int argc, const char** argv, const char** envv) {
    assert(argc);
    const char* ipath = NULL;
    if(strcmp(argv[0], "/user/dynld") == 0) {
        shift_args(&argc, &argv);
        ipath = shift_args(&argc, &argv);
        if(!ipath) {
            eprintfln("Missing input path!");
            return 1;
        }
    } else ipath = argv[0];
    Elf* main = NULL;
    intptr_t fd = open(ipath, O_RDONLY);
    if(fd < 0) {
        eprintfln("(dynld) Failed to open: %s", ipath);
        return 1;
    }
    assert(load_elf(&main, ipath, fd) >= 0);
    assert(relocate_elf(main) >= 0); 
    assert(main->header.entry);
    close(fd);
    return ((_start_t)(main->header.entry + main->base))(argc, argv, envv);
}

void _start(int argc, const char** argv, const char** envv) {
    _exit(main(argc, argv, envv));
}
