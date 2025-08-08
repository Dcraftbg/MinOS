#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct gtlist_head gtlist_head;
struct gtlist_head {
    gtlist_head *next, *prev;
};
// TODO: green threads might/should be able to set exit code maybe.
// I'm not entirely sure tho
typedef struct {
    gtlist_head list;
    void *sp, *sp_base;
    // Entry point + argument
    void (*entry)(void*);
    void *arg;
    // Poll
    uint32_t epoll_events;
} GThread;

void gtinit(void);
void gtyield(void);
void gtgo(void (*entry)(void* arg), void* arg);
GThread* gthread_current(void);
#define GTBLOCKIN  (1 << 0)
#define GTBLOCKOUT (1 << 1)
void gtblockfd(unsigned int fd, uint32_t events);

// TODO: atomic_bool for when we have hardware-threaded gt supported
typedef struct GTMutex {
    bool lock;
    gtlist_head list; // <- Threads blocking on mutex
} GTMutex;
void gtmutex_init(GTMutex* mutex);
void gtmutex_lock(GTMutex* mutex);
void gtmutex_unlock(GTMutex* mutex);
#define gtmutex_scoped(__mutex) for(int _once_counter = (gtmutex_lock(__mutex), 0); _once_counter < 1; _once_counter++, gtmutex_unlock(__mutex))


#ifdef GT_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifdef _WIN32

#else
# include <sys/epoll.h>
# include <sys/mman.h>
#endif


static void gtlist_init(gtlist_head *list) {
    list->next = list->prev = list;
}
static void gtlist_insert(gtlist_head *list, gtlist_head *new_list) {
    new_list->prev = list->prev;
    new_list->next = list;
    new_list->prev->next = new_list;
    new_list->next->prev = new_list;
}
static bool gtlist_empty(gtlist_head *list) {
    return list->next == list;
}
static void gtlist_remove(gtlist_head *list) {
    list->prev->next = list->next;
    list->next->prev = list->prev;
    list->next = list;
    list->prev = list;
}
static gtlist_head *gtlist_next(gtlist_head *list) {
    return list->next != list ? list->next : NULL;
}
typedef struct GPollBucket GPollBucket;
struct GPollBucket {
    GPollBucket* next;
    int fd;
    uint32_t epoll_events;
    gtlist_head threads;
};
typedef struct {
    struct {
        GPollBucket **items;
        size_t len;
    } buckets;
    size_t len;
} GPollMap;
static bool gpoll_map_reserve(GPollMap* map, size_t extra) {
    if(map->len + extra > map->buckets.len) {
        size_t ncap = map->buckets.len*2 + extra;
        GPollBucket** newbuckets = malloc(sizeof(*newbuckets)*ncap);
        if(!newbuckets) return false;
        memset(newbuckets, 0, sizeof(*newbuckets) * ncap);
        for(size_t i = 0; i < map->buckets.len; ++i) {
            GPollBucket* oldbucket = map->buckets.items[i];
            while(oldbucket) {
                GPollBucket* next = oldbucket->next;
                size_t hash = ((size_t)oldbucket->fd) % ncap;
                GPollBucket* newbucket = newbuckets[hash];
                oldbucket->next = newbucket;
                newbuckets[hash] = oldbucket;
                oldbucket = next;
            }
        }
        free(map->buckets.items);
        map->buckets.items = newbuckets;
        map->buckets.len = ncap;
    }
    return true;
}
static GPollBucket* gpoll_map_insert(GPollMap* map, int fd) {
    if(!gpoll_map_reserve(map, 1)) return NULL;
    size_t hash = ((size_t)fd) % map->buckets.len;
    GPollBucket* into = map->buckets.items[hash];
    while(into) {
        if(into->fd == fd) return into;
        into = into->next;
    }
    GPollBucket* bucket = malloc(sizeof(*bucket));
    if(!bucket) return NULL;
    bucket->next = NULL;
    bucket->epoll_events = 0;
    bucket->fd = fd;
    gtlist_init(&bucket->threads);
    bucket->next = into;
    map->buckets.items[hash] = bucket;
    map->len++;
    return bucket;
}
static GPollBucket* gpoll_map_remove(GPollMap* map, int fd) {
    if(!map->buckets.len) return NULL;
    size_t hash = ((size_t)fd) % map->buckets.len;
    GPollBucket *bucket = map->buckets.items[hash], **prev_next = &map->buckets.items[hash];
    while(bucket) {
        if(bucket->fd == fd) {
            *prev_next = bucket->next;
            map->len--;
            return bucket;
        }
        prev_next = &bucket->next;
        bucket = bucket->next;
    }
    return NULL;
}
typedef struct {
    gtlist_head queue;
    gtlist_head dead;
    GPollMap pollmap;
    GThread* thread_current;
    uintptr_t epoll;
} GScheduler;
static GScheduler scheduler = { 0 };
GThread* gthread_current(void) { 
    return scheduler.thread_current;
}

// Architecture specific
void __attribute__((naked)) gtyield(void) {
#if defined(__aarch64__)
    asm(
        "sub sp, sp, 112\n"
        "stp x19, x20, [sp, #0]\n"
        "stp x21, x22, [sp, #16]\n"
        "stp x23, x24, [sp, #32]\n"
        "stp x25, x26, [sp, #48]\n"
        "stp x27, x28, [sp, #64]\n"
        "stp x29, x30, [sp, #80]\n"
        "stp x29, x30, [sp, #96]\n"
        "mov x0, sp\n"
        "bl gtswitch\n"
        "mov sp, x0\n"
        "ldp x19, x20, [sp, #0]\n"
        "ldp x21, x22, [sp, #16]\n"
        "ldp x23, x24, [sp, #32]\n"
        "ldp x25, x26, [sp, #48]\n"
        "ldp x27, x28, [sp, #64]\n"
        "ldp x29, x30, [sp, #80]\n"
        "ldp x29, x30, [sp, #96]\n"
        "add sp, sp, 112\n"
        "ret"
    );
#elif defined(__x86_64__)
# ifdef _WIN32
    asm(
        "push %rbp\n"
        "push %rbx\n"
        "push %r12\n"
        "push %r13\n"
        "push %r14\n"
        "push %r15\n"
        "push %rsi\n"
        "push %rdi\n"
        "movq %rsp, %rcx\n"
        "call gtswitch\n"
        "mov %rax, %rsp\n"
        "pop %rdi\n"
        "pop %rsi\n"
        "pop %r15\n"
        "pop %r14\n"
        "pop %r13\n"
        "pop %r12\n"
        "pop %rbx\n"
        "pop %rbp\n"
        "ret"
    );
# else
    asm(
        "push %rbp\n"
        "push %rbx\n"
        "push %r12\n"
        "push %r13\n"
        "push %r14\n"
        "push %r15\n"
        "movq %rsp, %rdi\n"
        "call gtswitch\n"
        "mov %rax, %rsp\n"
        "pop %r15\n"
        "pop %r14\n"
        "pop %r13\n"
        "pop %r12\n"
        "pop %rbx\n"
        "pop %rbp\n"
        "ret"
    );
# endif
#elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    asm(
        "push %ebp\n"
        "push %ebx\n"
        "push %esi\n"
        "push %edi\n"
        "mov %esp, %ecx\n"
        "push %ecx\n"
        "call gtswitch\n"
        "mov %eax, %esp\n"
        "pop %edi\n"
        "pop %esi\n"
        "pop %ebx\n"
        "pop %ebp\n"
        "ret"
    );
#else
# error "Please port gtyield to your unknown Architecture"
#endif 
}

static void gtprelude(void);
static void* gtsetup_frame(void* sp_void) {
    uintptr_t* sp = sp_void;
#if defined(__aarch64__) || defined(__arm64__)
    sp -= 14;
    memset(sp, 0, 14*sizeof(uintptr_t));
    sp[13] = (uintptr_t)gtprelude;
#elif defined(__x86_64__)
    *(--sp) = 0; // Reserve some memory for alignment :O
    *(--sp) = (uintptr_t)gtprelude; // rip
    *(--sp) = 0;         // rbp
    *(--sp) = 0;         // rbx
    *(--sp) = 0;         // r12
    *(--sp) = 0;         // r13
    *(--sp) = 0;         // r14
    *(--sp) = 0;         // r15
# ifdef _WIN32
    *(--sp) = 0;         // rsi
    *(--sp) = 0;         // rdi
# endif
#elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
    *(--sp) = 0; // Reserve some memory for alignment :O
    *(--sp) = (uintptr_t)gtprelude; // rip
    *(--sp) = 0;         // ebp
    *(--sp) = 0;         // ebx
    *(--sp) = 0;         // esi
    *(--sp) = 0;         // edi
#else
# error "Please port gtsetup_frame to your unknown Architecture"
#endif
    return sp;
}
// End Architecture specific
#define GT_STACK_SIZE (4 * 4096)
// TODO: Maybe handle out of memory gracefully? I'm not too sure
void gtgo(void (*entry)(void* arg), void* arg) {
    GThread* thread;
    if(gtlist_empty(&scheduler.dead)) {
        thread = malloc(sizeof(GThread));
        assert(thread && "Ran out of memory");
        gtlist_init(&thread->list);
    #ifdef _WIN32
        thread->sp_base = VirtualAlloc(NULL, GT_STACK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (thread->sp == NULL) {
            fprintf(stderr, "VirtualAlloc failed: %lu\n", GetLastError());
            exit(1);
        }
        thread->sp_base = (char*)thread->sp + GT_STACK_SIZE;
    #else
        thread->sp_base = mmap(NULL, GT_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(thread->sp == MAP_FAILED) {
            perror("mmap stack");
            exit(1);
        }
        thread->sp_base += GT_STACK_SIZE;
    #endif
    } else {
        thread = (GThread*)scheduler.dead.next;
        gtlist_remove(&thread->list);
    }
    thread->sp = gtsetup_frame(thread->sp_base);
    thread->entry = entry;
    thread->arg = arg;
    gtlist_insert(&scheduler.queue, &thread->list);
}
// TODO: gtpurge function that purges all dead threads
void* gtswitch(void* sp) {
    gthread_current()->sp = sp; // Save the last context
    if(scheduler.pollmap.len > 0) {
        int timeout = 0;
        // TODO: maybe allow threads to register a timeout.
        // for our purposes its fine but in general it might be useful :)
        if(gtlist_empty(&scheduler.queue)) timeout = -1;
        do {
            struct epoll_event events[128];
            int n;
            do {
            n = epoll_wait(
    #ifdef _WIN32
                (HANDLE)
    #endif            
                scheduler.epoll, events, sizeof(events)/sizeof(*events), timeout);
            } while (n < 0 && errno == EINTR);
            assert(n >= 0);
            for(size_t i = 0; i < (size_t)n; ++i) {
                GPollBucket* bucket = events[i].data.ptr;
                gtlist_head* next;
                for(gtlist_head* head = bucket->threads.next; head != &bucket->threads; head = next) {
                    next = head->next;
                    GThread* thread = (GThread*)head;
                    if(thread->epoll_events & events[i].events) {
                        gtlist_remove(&thread->list);
                        gtlist_insert(&scheduler.queue, &thread->list);
                    }
                }
                struct epoll_event ev;
                ev.events = bucket->epoll_events & (~events[i].events);
                ev.data.ptr = bucket;
                int fd = bucket->fd;
                int op = EPOLL_CTL_MOD;
                if(gtlist_empty(&bucket->threads)) {
                    free(gpoll_map_remove(&scheduler.pollmap, bucket->fd));
                    op = EPOLL_CTL_DEL;
                }
                if(epoll_ctl(
    #ifdef _WIN32
                (HANDLE)
    #endif
                    scheduler.epoll, op, fd, &ev) < 0) {
                    perror("mod/del fd in epoll");
                    exit(1);
                }
                // gtlist_remove(&ethread->list);
                // gtlist_insert(&ethread->list, &scheduler.queue);
            }
        } while(gtlist_empty(&scheduler.queue));
    }
    GThread* thread = (GThread*)gtlist_next(&scheduler.queue);
    gtlist_remove(&thread->list);
    gtlist_insert(&scheduler.queue, &thread->list); // <- Add it to the end
    scheduler.thread_current = thread;
    return thread->sp;
}
static void gtprelude(void) {
    GThread* thread = gthread_current();
    thread->entry(thread->arg);
    gtlist_remove(&thread->list);
    gtlist_insert(&scheduler.dead, &thread->list);
    gtyield();
    assert(false && "unreachable");
}

void gtblockfd(unsigned int fd, uint32_t events) {
    GThread* thread = gthread_current();
    thread->epoll_events = 0;
#ifdef _WIN32
    thread->epoll_events |= EPOLLHUP | EPOLLERR;
#endif
    if(events & GTBLOCKIN)  thread->epoll_events |= EPOLLIN;
    if(events & GTBLOCKOUT) thread->epoll_events |= EPOLLOUT;
    assert(thread->epoll_events);
    // TODO: Find a way to reuse this sheizung instead of adding and removing it.
    // Its cheap to add and remove but still.
    GPollBucket* bucket = gpoll_map_insert(&scheduler.pollmap, fd);
    assert(bucket);
    int op = EPOLL_CTL_MOD;
    if(bucket->epoll_events == 0) op = EPOLL_CTL_ADD;
    if(bucket->epoll_events != thread->epoll_events) {
        bucket->epoll_events |= thread->epoll_events;
        struct epoll_event ev;
        ev.events = bucket->epoll_events;
        ev.data.ptr = bucket;
        if(epoll_ctl(
#ifdef _WIN32
            (HANDLE)
#endif
            scheduler.epoll, op, fd, &ev) < 0) {
            perror("adding fd in epoll");
            exit(1);
        }
    }

    gtlist_remove(&thread->list);
    gtlist_insert(&bucket->threads, &thread->list);
    gtyield();
}
void gtinit(void) {
    gtlist_init(&scheduler.queue);
    gtlist_init(&scheduler.dead);
    intptr_t e = (intptr_t)epoll_create1(0);
    assert(e >= 0);
    scheduler.epoll = e;
    GThread* main_thread = malloc(sizeof(GThread));
    assert(main_thread && "Ran out of memory");
    main_thread->sp = 0;
    gtlist_init(&main_thread->list);
    gtlist_insert(&scheduler.queue, &main_thread->list);
    scheduler.thread_current = main_thread;
}
void gtmutex_init(GTMutex* mutex) {
    mutex->lock = false;
    gtlist_init(&mutex->list);
}
void gtmutex_lock(GTMutex* mutex) {
    if(mutex->lock) {
        GThread* thread = gthread_current();
        gtlist_remove(&thread->list);
        gtlist_insert(&mutex->list, &thread->list);
        gtyield();
    } else mutex->lock = true;
}
void gtmutex_unlock(GTMutex* mutex) {
    GThread* blocking_thread = (GThread*)gtlist_next(&mutex->list);
    if(blocking_thread) {
        gtlist_remove(&blocking_thread->list);
        gtlist_insert(&scheduler.queue, &blocking_thread->list);
    } else mutex->lock = false;
}
#endif
