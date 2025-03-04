#include "epoll.h"
#include "mem/slab.h"
#include "assert.h"
#include <minos/status.h>
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
}
