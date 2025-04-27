#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <minos/sysstd.h>
#include <collections/list.h>
static void (*atexit_funcs[ATEXIT_MAX])(void);
static size_t atexit_funcs_count = 0;
void exit(int64_t code) {
    for(size_t i = atexit_funcs_count; i > 0; --i) {
        atexit_funcs[i-1]();
    }
    _exit(code);
    for(;;);
}
#include <assert.h>
int atexit(void (*function)(void)) {
    // TODO: Report error for this instead of crash
    assert(atexit_funcs_count < ATEXIT_MAX);
    // TODO: As far as I can tell registering multiple of the same function should only register it once
    // This is not something we do. We should fix that at some point:tm:
    atexit_funcs[atexit_funcs_count++] = function;
    return 0;
}
void abort() {
    _exit(-1);
    for(;;);
}

#include <minos/heap.h>
typedef struct {
    size_t id;
    struct list alloc_list;
    MinOSHeap heap;
} _LibcInternalHeap;
#define _LIBC_INTERNAL_HEAPS_MAX 5
#define _LIBC_INTERNAL_INVALID_HEAPID ((size_t)-1)
_LibcInternalHeap _libc_internal_heaps[_LIBC_INTERNAL_HEAPS_MAX];
typedef struct _HeapNode _HeapNode;
// TODO: Migrate to a plist mechanism
struct _HeapNode {
    struct list list; 
    bool free;
    size_t size;
    char data[];
};

#include <assert.h>
#define MIN_HEAP_BLOCK_SIZE (sizeof(_HeapNode)+16)
#define alignup_to(n, size)   ((((n)+((size)-1))/(size))*(size))
#define aligndown_to(n, size) (((n)/(size))*(size))
bool libc_heap_extend(_LibcInternalHeap* heap, size_t extra) {
    // TODO: Maybe ask for page size instead
    size_t page_extend = alignup_to(alignup_to(extra + sizeof(_HeapNode), sizeof(_HeapNode)), 4096);
    intptr_t e;
    if((e=heap_extend(heap->id, page_extend)) < 0) return false;
    _HeapNode* end = (_HeapNode*)(heap->heap.address+heap->heap.size);
    size_t n = heap->heap.size;
    assert((e=heap_get(heap->id, &heap->heap)) >= 0);
    list_init(&end->list);
    end->size = heap->heap.size-n-sizeof(_HeapNode);
    end->free = true;
    list_append(&end->list, heap->alloc_list.prev);
    return true;
}
void* libc_heap_allocate_within(_LibcInternalHeap* heap, size_t size) {
    for(_HeapNode* node=(_HeapNode*)heap->alloc_list.next; &node->list != &heap->alloc_list; node = (_HeapNode*)node->list.next) {
        if(node->size >= size && node->free) {
            size_t left = node->size-size;
            _HeapNode* next = NULL;
            if(left > MIN_HEAP_BLOCK_SIZE) {
                next = (_HeapNode*)(node->data+size);
                // TODO: Align to 16
                list_init(&next->list);
                next->free = true;
                next->size = left-sizeof(_HeapNode);
                list_append(&next->list, &node->list);
                node->size = size;
            }
            node->free = false;
            return node->data;
        }
    }
    return NULL;
}
bool libc_heap_find_alloc(_LibcInternalHeap* heap, void* addr, size_t *size) {
    for(_HeapNode* node=(_HeapNode*)heap->alloc_list.next; &node->list != &heap->alloc_list; node = (_HeapNode*)node->list.next) {
        if(node->data == addr) {
            *size = node->size;
            return true;
        }
    }
    return false;
}
void* libc_heap_allocate(_LibcInternalHeap* heap, size_t size) {
    if(heap->heap.size < sizeof(_HeapNode)) return NULL;
    void* addr=libc_heap_allocate_within(heap, size);
    if(addr) return addr;
    if(!libc_heap_extend(heap, size)) return NULL;
    return libc_heap_allocate_within(heap, size);
}

void* libc_heap_reallocate(_LibcInternalHeap* heap, void* oldaddr, size_t newsize) {
    if(heap->heap.size < sizeof(_HeapNode)) return NULL;
    void* newaddr = libc_heap_allocate(heap, newsize);
    if(!newaddr) return NULL;
    size_t oldsize;
    assert(libc_heap_find_alloc(heap, oldaddr, &oldsize) && "Invalid allocation passed to realloc");
    if(oldsize >= newsize) oldsize=newsize;
    memcpy(newaddr, oldaddr, oldsize);
    free(oldaddr);
    return newaddr;
}
// TODO: faster Deallocation infering address is a valid heap address
void libc_heap_deallocate(_LibcInternalHeap* heap, void* address) {
    if(heap->heap.size < sizeof(_HeapNode)) return;
    for(_HeapNode* node=(_HeapNode*)heap->alloc_list.next; &node->list != &heap->alloc_list; node = (_HeapNode*)node->list.next) {
        if(node->data == address) {
            assert((!node->free) && "Double free");
            node->free = true;
            _HeapNode* next = (_HeapNode*)node->list.next;
            while(&next->list != &heap->alloc_list && next->free) {
                struct list* next_ptr = next->list.next;
                list_remove(&next->list);
                assert((char*)next == node->data+node->size);
                node->size += next->size + sizeof(_HeapNode);
                next = (_HeapNode*)next_ptr;
            }
            _HeapNode* prev = (_HeapNode*)node->list.prev;
            while(&prev->list != &heap->alloc_list && prev->free) {
                assert((char*)node == prev->data+prev->size);
                prev->size += node->size + sizeof(_HeapNode);
                list_remove(&node->list);
                node = prev;
                prev = (_HeapNode*)node->list.prev;
            }
            return;
        }
    }
    assert(false && "Invalid address to free");
    // TODO: Error here. Invalid address
}

void init_libc_heap(_LibcInternalHeap* heap) {
    // Some sort of error
    if(heap->heap.size < sizeof(_HeapNode)) return;
    list_init(&heap->alloc_list);
    _HeapNode* node = (_HeapNode*)heap->heap.address;
    list_init(&node->list);
    node->free = true;
    node->size = aligndown_to(heap->heap.size-sizeof(_HeapNode), 16);
    list_append(&node->list, &heap->alloc_list);
}
#define LIBC_MIN_SIZE (4096*16)
void* malloc(size_t size) {
    size = alignup_to(size, 16);
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        if(_libc_internal_heaps[i].id == _LIBC_INTERNAL_INVALID_HEAPID) {
            intptr_t e = heap_create(HEAP_RESIZABLE, NULL, size > LIBC_MIN_SIZE ? size : LIBC_MIN_SIZE);
            if(e < 0) return NULL; // Failed to create heap. Most likely out of memory
            _libc_internal_heaps[i].id = e;
            e=heap_get(e, &_libc_internal_heaps[i].heap);
            if(e < 0) return NULL; // Really bad error. heap_create returned invalid id
            init_libc_heap(&_libc_internal_heaps[i]);
        }
        void* addr=libc_heap_allocate(&_libc_internal_heaps[i], size);
        if(addr) return addr;
    }
    // Reached limit
    return NULL;
}

void* calloc(size_t elm, size_t size) {
    void* buf = malloc(elm*size);
    if(buf) {
        memset(buf, 0, elm*size);
    }
    return buf;
}
#include <stdio.h>
void* realloc(void* addr, size_t newsize) {
    if(!addr) return malloc(newsize);
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        if(_libc_internal_heaps[i].id == _LIBC_INTERNAL_INVALID_HEAPID) continue;
        _LibcInternalHeap* heap = &_libc_internal_heaps[i];
        if(heap->heap.address <= addr && addr < heap->heap.address+heap->heap.size) {
            return libc_heap_reallocate(heap, addr, newsize);
        }
    }
    // Reached limit
    assert(false && "Invalid address to realloc");
}
// TODO: Smarter free with heap_get() and checking heap ranges
void free(void* addr) {
    if(!addr) return;
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        if(_libc_internal_heaps[i].id == _LIBC_INTERNAL_INVALID_HEAPID) continue;
        _LibcInternalHeap* heap = &_libc_internal_heaps[i];
        if(heap->heap.address <= addr && addr < heap->heap.address+heap->heap.size) {
            libc_heap_deallocate(heap, addr);
            return;
        }
    }
    // Reached limit
    assert(false && "Invalid address to free");
}
void _libc_internal_init_heap() {
    for(size_t i = 0; i < _LIBC_INTERNAL_HEAPS_MAX; ++i) {
        _libc_internal_heaps[i].id = _LIBC_INTERNAL_INVALID_HEAPID;
    }
}

#include <string.h>
int atoi(const char *str) {
    char* end;
    return strtol(str, &end, 10);
}
float atof(const char *str) {
    char* end;
    return strtod(str, &end);
}
int abs(int num) {
    return num < 0 ? -num : num;
}
long labs(long num) {
    return num < 0 ? -num : num;
}
long long llabs(long long num) {
    return num < 0 ? -num : num;
}
long strtol(const char* nptr, char** endptr, int base) {
    return strtoll(nptr, endptr, base);
}

long long strtoll(const char* nptr, char** endptr, int base) {
    if(endptr) *endptr = (char*)nptr;
    while(isspace(*nptr)) nptr++;
    if(!isxdigit(*nptr)) return 0;
    if(base == 0) {
        if(nptr[0] == '0' && nptr[1] == 'x') base = 16;
        else if(nptr[0] == '0') base = 8;
        else base = 10;
    }
    assert(base >= 2);
    long long sign = 1;
    if(nptr[0] == '-') {
        sign = -1;
        nptr++;
    }
    if(base == 16 && nptr[0] == '0' && nptr[1] == 'x') nptr += 2;
    long long result = 0;
    while(*nptr) {
        unsigned long long d = 10000;
        char c = *nptr;
        if(c >= '0' && c <= '9') d = c-'0';
        else if (c >= 'A' && c <= 'Z') d = c-'A' + 10;
        else if (c >= 'a' && c <= 'z') d = c-'a' + 10;
        if(d >= base) {
            if(endptr) *endptr = (char*)nptr;
            return result;
        }
        result *= (unsigned long long)base;
        result += d * sign;
        nptr++;
    }
    if(endptr) *endptr = (char*)nptr;
    return result;
}

static void qsort_swap(void* p1, void* p2, size_t size) {
    uint8_t* p1_b = p1;
    uint8_t* p2_b = p2;
    for(size_t i = 0; i < size; ++i) {
        uint8_t t = p1_b[i];
        p1_b[i] = p2_b[i];
        p2_b[i] = t;
    }
}
static void* qsort_part(void* ptr, void* low, void* high, size_t size, int (*comp)(const void*, const void*)) {
    void* pivot = high;
    if(high == ptr)
        return pivot;

    void* i = low;
    for(void* j = low; j < high; j += size) {
        if (comp(j, pivot) <= 0) {
            qsort_swap(i, j, size);
            i += size;
        }
    }
    qsort_swap(i, high, size);
    return i;
}
static void qsort_impl(void* ptr, void* low, void* high, size_t size, int (*comp)(const void*, const void*)) {
    if (low >= high)
        return;
    void* p = qsort_part(ptr, low, high, size, comp);
    qsort_impl(ptr, low     , p - size, size, comp);
    qsort_impl(ptr, p + size, high    , size, comp);
}
void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*)) {
    if(count == 0) 
        return;
    qsort_impl(ptr, ptr, ptr+(count-1)*size, size, comp);
}

int system(const char* command) {
    fprintf(stderr, "system() is a stub\n");
    exit(1);
}
void *bsearch(const void *key, const void *base, size_t nitems, size_t size, int (*compar)(const void *, const void *)) {
    fprintf(stderr, "bsearch() is a stub\n");
    exit(1);
}

char *mktemp(char *templat) {
    fprintf(stderr, "mktemp() is a stub\n");
    exit(1);
}

char *realpath(const char *path, char *resolved_path) {
    fprintf(stderr, "realpath() is a stub\n");
    exit(1);
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}
unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    if(base == 0) {
        if(nptr[0] == '0' && nptr[1] == 'x') base = 16;
        else if(nptr[0] == '0') base = 8;
        else base = 10;
    }
    assert(base >= 2);
    while(isspace(*nptr)) nptr++;
    if(base == 16 && nptr[0] == '0' && nptr[1] == 'x') nptr += 2;
    unsigned long long result = 0;
    while(*nptr) {
        unsigned long long d = 10000;
        char c = *nptr;
        if(c >= '0' && c <= '9') d = c-'0';
        else if (c >= 'A' && c <= 'Z') d = c-'A' + 10;
        else if (c >= 'a' && c <= 'z') d = c-'a' + 10;
        if(d >= base) {
            if(endptr) *endptr = (char*)nptr;
            return result;
        }
        result *= (unsigned long long)base;
        result += d;
        nptr++;
    }
    if(endptr) *endptr = (char*)nptr;
    return result;
}

double strtold(const char *nptr, char **endptr) {
    fprintf(stderr, "strtold() is a stub\n");
    exit(1);
}
float strtof(const char *nptr, char **endptr) {
    fprintf(stderr, "strtof() is a stub\n");
    exit(1);
}
