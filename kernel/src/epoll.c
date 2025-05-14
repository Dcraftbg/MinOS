#include "epoll.h"
#include "mem/slab.h"
#include "assert.h"
#include <minos/status.h>
#include "process.h"
#include "log.h"
static EpollFd* epoll_find(Epoll* epoll, int fd) {
    for(struct list* head = epoll->unready.next; head != &epoll->unready; head = head->next) {
        EpollFd* entry = (EpollFd*)head;
        if(entry->fd == fd) return entry;
    }
    return NULL;
}
intptr_t epoll_add(Epoll* epoll, EpollFd* fd) {
    if(epoll_find(epoll, fd->fd)) return -ALREADY_EXISTS;
    list_insert(&fd->list, &epoll->unready);
    return 0;
}
intptr_t epoll_del(Epoll* epoll, int fd) {
    EpollFd* entry;
    if(!(entry=epoll_find(epoll, fd))) return -NOT_FOUND; 
    list_remove(&entry->list);
    epoll_fd_delete(entry);
    return 0;
}
intptr_t epoll_mod(Epoll* epoll, int fd, const struct epoll_event* event) {
    EpollFd* entry;
    if(!(entry=epoll_find(epoll, fd))) return -NOT_FOUND; 
    entry->event = *event;
    return 0;
}
static Cache* epoll_fd_cache = NULL;
static Cache* epoll_cache = NULL;
EpollFd* epoll_fd_new(int fd, const struct epoll_event* event) {
    EpollFd* entry = cache_alloc(epoll_fd_cache);
    if(!entry) return NULL;
    entry->fd = fd;
    entry->event = *event;
    return entry;
}
void epoll_fd_delete(EpollFd* fd) {
    cache_dealloc(epoll_fd_cache, fd);
}
void init_epoll_cache(void) {
    assert(epoll_fd_cache = create_new_cache(sizeof(EpollFd), "EpollFd"));
    assert(epoll_cache = create_new_cache(sizeof(Epoll), "Epoll"));
}
bool epoll_poll(Epoll* epoll, Process* process) {
    debug_assert(list_empty(&epoll->ready));
    bool terminal = false;
    struct list *next;
    for(struct list *head = epoll->unready.next; head != &epoll->unready; head = next) {
        next = head->next;
        EpollFd* fd = (EpollFd*)head;
        Resource* res = resource_find_by_id(process->resources, fd->fd);
        if(!res) {
            kwarn("epoll p%zu> Failed to find resource: %zu", process->id, fd->fd);
            terminal = true;
            continue;
        }
        switch(res->kind) {
        case RESOURCE_INODE:
            fd->result_events = 0;
            if((fd->event.events & EPOLLIN) && inode_is_readable(res->as.inode.inode)) {
                fd->result_events |= EPOLLIN;
            } else if ((fd->event.events & EPOLLOUT) && inode_is_writeable(res->as.inode.inode)) {
                fd->result_events |= EPOLLOUT;
            } else if ((fd->event.events & EPOLLHUP) && inode_is_hungup(res->as.inode.inode)) {
                fd->result_events |= EPOLLHUP;
            } else continue; // <- We didn't get any event. Shortcircuit 
            if(fd->result_events) {
                list_remove(head);
                list_insert(head, &epoll->ready);
                terminal = true;
            }
            break;
        default:
            kwarn("epoll p%zu> Non inode resource in epoll: %zu", process->id, fd->fd);
            terminal = true;
        }
    }
    return terminal;
}

static void epoll_cleanup(Inode* inode) {
    Epoll* epoll = (Epoll*)inode;
    struct list *next;
    for(struct list *head = epoll->unready.next; head != &epoll->unready; head = next) {
        next = head->next;
        list_remove(head);
        cache_dealloc(epoll_fd_cache, (EpollFd*)head);
    }
    for(struct list *head = epoll->ready.next; head != &epoll->ready; head = next) {
        next = head->next;
        list_remove(head);
        cache_dealloc(epoll_fd_cache, (EpollFd*)head);
    }
}
static InodeOps epoll_ops = {
    .cleanup = epoll_cleanup,
};
Epoll* epoll_new(void) {
    Epoll* epoll = cache_alloc(epoll_cache);
    if(!epoll) return NULL;
    inode_init(&epoll->inode, epoll_cache);
    epoll->inode.kind = INODE_EPOLL;
    list_init(&epoll->unready);
    list_init(&epoll->ready);
    epoll->inode.ops = &epoll_ops;
    return epoll;
}
