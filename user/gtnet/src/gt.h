#pragma once
#include <collections/list.h>
// TODO: green threads might/should be able to set exit code maybe.
// I'm not entirely sure tho
typedef struct {
    struct list list;
    void* sp;
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

#ifdef GT_IMPLEMENTATION
typedef struct GPollBucket GPollBucket;
struct GPollBucket {
    GPollBucket* next;
    int fd;
    uint32_t epoll_events;
    struct list threads;
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
    list_init(&bucket->threads);
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
    struct list queue;
    struct list dead;
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
    asm(
        "push %rbp\n"
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
        "pop %rbp\n"
        "ret"
    );
}

static void gtprelude(void);
static void* gtsetup_frame(void* sp_void) {
    uint64_t* sp = sp_void;
    *(--sp) = (uint64_t)gtprelude; // rip
    *(--sp) = 0;         // rbp
    *(--sp) = 0;         // r12
    *(--sp) = 0;         // r13
    *(--sp) = 0;         // r14
    *(--sp) = 0;         // r15
    return sp;
}
// End Architecture specific
#define GT_STACK_SIZE (4 * 4096)
// TODO: Maybe handle out of memory gracefully? I'm not too sure
// TODO: Reuse threads from dead
void gtgo(void (*entry)(void* arg), void* arg) {
    GThread* thread =  malloc(sizeof(GThread));
    assert(thread && "Ran out of memory");
#ifdef _MINOS
    intptr_t e = heap_create(0, NULL, GT_STACK_SIZE);
    assert(e >= 0 && "Ran out of memory");
    MinOSHeap heap = { 0 };
    assert(heap_get((uintptr_t)e, &heap) >= 0);
    // Stack grows backwards:
    thread->sp = heap.address + heap.size; 
#else
#   error: Unsupported platform
#endif
    thread->sp = gtsetup_frame(thread->sp);
    list_init(&thread->list);
    thread->entry = entry;
    thread->arg = arg;
    list_insert(&thread->list, &scheduler.queue);
}
// TODO: gtpurge function that purges all dead threads
void* gtswitch(void* sp) {
    gthread_current()->sp = sp; // Save the last context
    if(scheduler.pollmap.len > 0) {
        int timeout = 0;
        // TODO: maybe allow threads to register a timeout.
        // for our purposes its fine but in general it might be useful :)
        if(list_empty(&scheduler.queue)) timeout = -1;
        struct epoll_event events[128];
        int n = epoll_wait(scheduler.epoll, events, sizeof(events)/sizeof(*events), timeout);
        assert(n >= 0);
        for(size_t i = 0; i < (size_t)n; ++i) {
            GPollBucket* bucket = events[i].data.ptr;
            struct list* next;
            for(struct list* head = bucket->threads.next; head != &bucket->threads; head = next) {
                next = head->next;
                GThread* thread = (GThread*)head;
                if(thread->epoll_events & events[i].events) {
                    list_remove(&thread->list);
                    list_insert(&thread->list, &scheduler.queue);
                }
            }
            struct epoll_event ev;
            ev.events = bucket->epoll_events & (~events[i].events);
            ev.data.ptr = bucket;
            int fd = bucket->fd;
            int op = EPOLL_CTL_MOD;
            if(list_empty(&bucket->threads)) {
                free(gpoll_map_remove(&scheduler.pollmap, bucket->fd));
                op = EPOLL_CTL_DEL;
            }
            if(epoll_ctl(scheduler.epoll, op, fd, &ev) < 0) {
                perror("mod/del fd in epoll");
                exit(1);
            }
            // list_remove(&ethread->list);
            // list_insert(&ethread->list, &scheduler.queue);
        }
    }
    GThread* thread = (GThread*)list_next(&scheduler.queue);
    list_remove(&thread->list);
    list_insert(&thread->list, &scheduler.queue); // <- Add it to the end
    scheduler.thread_current = thread;
    return thread->sp;
}
static void __attribute__((naked)) gtprelude(void) {
    GThread* thread = gthread_current();
    thread->entry(thread->arg);
    list_remove(&thread->list);
    list_insert(&thread->list, &scheduler.dead);
    gtyield();
    assert(false && "unreachable");
}

void gtblockfd(unsigned int fd, uint32_t events) {
    GThread* thread = gthread_current();
    thread->epoll_events = 0;
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
        if(epoll_ctl(scheduler.epoll, op, fd, &ev) < 0) {
            perror("adding fd in epoll");
            exit(1);
        }
    }

    list_remove(&thread->list);
    list_insert(&thread->list, &bucket->threads);
    gtyield();
}
void gtinit(void) {
    list_init(&scheduler.queue);
    list_init(&scheduler.dead);
    intptr_t e = epoll_create1(0);
    assert(e >= 0);
    scheduler.epoll = e;
    GThread* main_thread = malloc(sizeof(GThread));
    assert(main_thread && "Ran out of memory");
    main_thread->sp = 0;
    list_init(&main_thread->list);
    list_insert(&main_thread->list, &scheduler.queue);
    scheduler.thread_current = main_thread;
}
#endif
