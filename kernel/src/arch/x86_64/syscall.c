#include "syscall.h"
#include "print.h"
#include "timer.h"
#include "process.h"
#include "task.h"
#include "exec.h"
#include "log.h"
#include "benchmark.h"
#include "kernel.h"
#include "idt.h"
#include <minos/heap.h>
#include <minos/time.h>
#include <minos/mmap.h>
#include "mem/shared_mem.h"
#include "task_regs.h"

static intptr_t parse_path(Process* process, Path* res, const char* path) {
    switch(path[0]) {
    case '/':
        return parse_abs(path, res);
    case '.':
        path++;
        if(path[0] != '/') return -INVALID_PATH;
        path++;
    default: 
        res->from = process->curdir_inode; 
        res->path = path;
        return 0;
    }
}
#define strace(fmt, ...) ktrace("P%zu:T%zu "fmt, current_process()->id, current_task()->id, __VA_ARGS__)
#define strace1(fmt)     ktrace("P%zu:T%zu "fmt, current_process()->id, current_task()->id)
// TODO: Safety features like copy_to_userspace, copy_from_userspace
intptr_t sys_open(const char* path, oflags_t flags) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_open(\"%s\", %d, %d)", path, (int)mode, (int)flags);
#endif
    Path p;
    Process* current = current_process();
    size_t id = 0;
    intptr_t e = 0;
    if((e=parse_path(current, &p, path)) < 0) return e;
    Resource* resource = resource_add(current->resources, &id);
    if(!resource) return -NOT_ENOUGH_MEM;
    resource->flags = flags;
    Inode* inode;
    if((e=vfs_find(&p, &inode)) < 0) {
        if(e == -NOT_FOUND && flags & O_CREAT) {
            if((e=vfs_creat(&p, flags & O_DIRECTORY, &inode)) < 0) return e;
            goto found;
        }
        resource_remove(current->resources, id);
        return e;
    }
found:
    if((flags & O_TRUNC) && (e=inode_truncate(inode, 0)) < 0) {
        idrop(inode);
        resource_remove(current->resources, id);
        return e;
    }
    resource->inode = inode;
    resource->offset = 0;
    return id;
}
intptr_t sys_write(uintptr_t handle, const void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_write(%lu, %p, %zu)", handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(!(res->flags & O_WRITE)) return -PERMISION_DENIED;
    intptr_t e;
    if((e=inode_write(res->inode, buf, size, res->offset)) < 0) return e;
    res->offset += e;
    return e;
}

intptr_t sys_get_dir_entries(uintptr_t handle, void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_get_dir_entries(%lu, %p, %zu)", handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(!(res->flags & O_READ)) return -PERMISION_DENIED;
    intptr_t e;
    size_t read_bytes;
    if((e=inode_get_dir_entries(res->inode, buf, size, res->offset, &read_bytes)) < 0) return e;
    res->offset += e;
    return read_bytes;
}
intptr_t sys_read(uintptr_t handle, void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_read(%lu, %p, %zu)", handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(!(res->flags & O_READ)) return -PERMISION_DENIED;
    intptr_t e;

    if(!(res->flags & O_NOBLOCK) && !inode_is_readable(res->inode)) block_is_readable(current_task(), res->inode);
    if((e=inode_read(res->inode, buf, size, res->offset)) < 0) return e;
    res->offset += e;
    return e;
}

intptr_t sys_ioctl(uintptr_t handle, Iop op, void* arg) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_ioctl(%lu, %zu, %p)", handle, op, arg);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    return inode_ioctl(res->inode, op, arg);
}

intptr_t sys_mmap(void** addr_ptr, size_t length, uint32_t prot, uint32_t flags, uintptr_t fd, off_t offset) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_mmap(%lu, %p, %zu)", handle, addr, size);
#endif
    Process* process = current_process();
    Task* task = current_task();
    void* addr = *addr_ptr;
    intptr_t e = 0;
    if(flags & MAP_ANONYMOUS) {
        pageflags_t pageflags = KERNEL_PFLAG_USER | KERNEL_PTYPE_USER |
                                KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE;
        if(prot & PROT_WRITE) {
            pageflags |= KERNEL_PFLAG_WRITE;
        }
        MemoryRegion* region = memregion_new(pageflags, 0, 0);
        if(!region) return -NOT_ENOUGH_MEM;
        MemoryList* list = memlist_new(region);
        if(!list) {
            memregion_drop(region, NULL);
            return -NOT_ENOUGH_MEM;
        }
        size_t pages_min = PAGE_ALIGN_UP(length) / PAGE_SIZE;
        size_t pages_max = pages_min;
        if((flags & MAP_FIXED_NOREPLACE) || (flags & MAP_FIXED)) pages_max = 0xFFFFFFFFFFFFFLL;
        if(addr == NULL) addr = (void*)task->eoe;
        // FIXME: Region must be shared between all tasks and must be available in all tasks
        MemoryList* insert_into = memlist_find_available(&task->memlist, region, addr, pages_min, pages_max);
        if(!insert_into) {
            memlist_dealloc(list, NULL);
            return -NOT_ENOUGH_MEM;
        }
        // TODO: differentiate fixed
        if((flags & MAP_FIXED_NOREPLACE) || (flags & MAP_FIXED)) {
            if(region->address > (uintptr_t)addr || region->address + (region->pages*PAGE_SIZE) < ((uintptr_t)addr + pages_min * PAGE_SIZE)) {
                memlist_dealloc(list, NULL);
                return -VIRTUAL_SPACE_OCCUPIED;
            }
            region->address = (uintptr_t)addr;
            region->pages = pages_min;
        }
        if(!page_alloc(task->cr3, region->address, region->pages, region->pageflags)) {
            memlist_dealloc(list, NULL);
            return -NOT_ENOUGH_MEM;
        }
        invalidate_pages((void*)region->address, region->pages);
        list_append(&list->list, &insert_into->list);
        addr = (void*)region->address;
    } else {
        // FIXME: don't just completely ignore flags
        Resource* res = resource_find_by_id(process->resources, fd);
        if(!res) return -INVALID_HANDLE;
        MmapContext context = {
            .page_table = task->cr3,
            .memlist = &task->memlist
        };
        intptr_t e = inode_mmap(res->inode, &context, &addr, PAGE_ALIGN_UP(length) / PAGE_SIZE);
        if(e < 0) return e;
        invalidate_pages(addr, PAGE_ALIGN_UP(length)/PAGE_SIZE);
    }
    *addr_ptr = addr;
    return e;
}
intptr_t sys_seek(uintptr_t handle, off_t offset, seekfrom_t from) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_seek(%lu, %d, %zu)", handle, (int)offset, from);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->inode->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    // TODO: Convertions with lba
    switch(from) {
    case SEEK_START:
        res->offset = offset;
        return res->offset;
    case SEEK_CURSOR:
        res->offset += offset;
        return res->offset;
    case SEEK_EOF: {
        intptr_t e;
        if((e=inode_size(res->inode)) < 0) return e;
        res->offset = e+offset;
        return res->offset;
    } break;
    default:
        return -INVALID_PARAM;
    }
}
intptr_t sys_tell(uintptr_t handle) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_tell(%lu)", handle);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    // TODO: Remove this check.
    // awweqweqw
    if(res->inode->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    return res->offset;
}
// TODO: Smarter resource sharing logic. No longer sharing Resource itself but the Inode+offset.
// The res->shared check is entirely useless
intptr_t sys_close(uintptr_t handle) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_close(%lu)",handle);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    idrop(res->inode);
    resource_remove(current->resources, handle);
    return 0;
}
intptr_t sys_fork() {
#ifdef CONFIG_LOG_SYSCALLS
    strace1("sys_fork()");
#endif
    intptr_t e;
    Process* current_proc = current_process();
    int child_index = -1;
    for(size_t i = 0; i < ARRAY_LEN(current_proc->children); ++i) {
        if(child_process_get_id(current_proc->children[i]) == INVALID_PROCESS_ID) {
            child_index = i;
            break;
        }
    }
    if(child_index < 0) {
        kwarn("(%zu) is trying to spawn too many child processes.", current_proc->id);
        return -LIMITS;
    }
    // TODO: Maybe put in a separate function like:
    // process_clone_image()
    Process* process = kernel_process_add();
    if(!process) return -LIMITS; // Reached max tasks and/or we're out of memory
    child_process_set_id(current_proc->children[child_index], process->id);
    process->parent = current_proc;
    process->resources = resourceblock_clone(current_proc->resources);
    if(!process->resources) {
        process_drop(process);
        return -NOT_ENOUGH_MEM;
    }
    process->curdir = kernel_malloc(PATH_MAX);
    if(!process->curdir) {
        process_drop(process);
        return -NOT_ENOUGH_MEM;
    }
    memcpy(process->curdir, current_proc->curdir, PAGE_SIZE);
    process->curdir_inode = iget(current_proc->curdir_inode);

    Task* current = current_task();
    Task* task = kernel_task_add();
    if(!task) {
        process_drop(process);
        return -LIMITS;
    } 
    process->main_thread = task;
    task->process = process;

    if((e=fork_trampoline(current, task)) < 0) {
        process_drop(process);
        drop_task(task);
        return e;
    }
    Task* now = current_task();
    if(now->id == task->id) {
        return -YOU_ARE_CHILD;
    }
    return process->id;
}
intptr_t sys_exec(const char* path, const char** argv, size_t argc, const char** envv, size_t envc) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_exec(%s, %p, %zu)", path, argv, argc);
#endif
    intptr_t e;
    Process* cur_proc = current_process();
    Task* cur_task = current_task();
    assert(cur_proc->main_thread == cur_task && "Exec on multiple threads is not supported yet");
    Task* task = kernel_task_add();
    if(!task) {
        return -LIMITS;
    }
    task->process = cur_proc;
    Args args=create_args(argc, argv);
    Args env =create_args(envc, envv);
    Path p;
    if((e=parse_path(cur_proc, &p, path)) < 0) {
        drop_task(task);
        return e;
    }
    if((e=exec(task, &p, &args, &env)) < 0) {
        drop_task(task);
        return e;
    }
    cur_task->flags &= ~(TASK_FLAG_PRESENT);
    cur_task->flags |= TASK_FLAG_DYING;
    cur_proc->main_thread = task;
    task->flags |= TASK_FLAG_PRESENT;
    // TODO: thread yield
    for(;;) asm volatile("hlt");
    return 0;
}

void sys_exit(int code) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_exit(%d)", code);
#endif
    Process* cur_proc = current_process();
    Task* cur_task = current_task();
    Process* parent_proc = cur_proc->parent;
    disable_interrupts();
    if(!parent_proc) {
        kwarn("Called exit on init process maybe? Thats kind of bad.");
        kwarn(" process_id: %zu", cur_proc->id);
        kwarn(" exit code : %d" , code);
        goto end;
    }
    for(size_t i = 0; i < ARRAY_LEN(parent_proc->children); ++i) {
        if(child_process_get_id(parent_proc->children[i]) == cur_proc->id) {
            child_process_mark_dead(parent_proc->children[i]);
            child_process_set_exit_code(parent_proc->children[i], code);
            goto end;
        }
    }
    kwarn("Hey. We couldn't find ourselves in the children list of the parent process. This is odd");
end:
    // TODO: Remove all child tasks.
    list_remove(&cur_task->list);
    {
        ResourceBlock* block = cur_proc->resources;
        while(block) {
            ResourceBlock* next = block->next;
            for(size_t i = 0; i < RESOURCES_PER_BLOCK; ++i) {
                if(block->data[i]) {
                    idrop(block->data[i]->inode);
                    cache_dealloc(kernel.resource_cache, block->data[i]);
                }
            }
            kernel_dealloc(block, sizeof(*block));
            block = next;
        }
    }
    {
        for(size_t i = 0; i < cur_proc->shared_memory.len; ++i) {
            if(cur_proc->shared_memory.items[i]) {
                shmdrop(cur_proc->shared_memory.items[i]);
            }
        }
        kernel_dealloc(cur_proc->shared_memory.items, cur_proc->shared_memory.cap * PAGE_SIZE);
    }
    cur_task->flags &= ~(TASK_FLAG_PRESENT);
    cur_task->flags |= TASK_FLAG_DYING;
    cur_proc->flags |= PROC_FLAG_DYING;
    enable_interrupts();
    // TODO: thread yield
    for(;;) asm volatile("hlt");
}

intptr_t sys_waitpid(size_t pid) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_waitpid(%zu)", pid);
#endif
    Process* cur_proc = current_process();
    Task* cur_task = current_task();
    int child_index=-1;
    for(size_t i = 0; i < ARRAY_LEN(cur_proc->children); ++i) {
        if(child_process_get_id(cur_proc->children[i]) == pid) {
            child_index = i;
            break;
        }
    }
    if(child_index < 0) {
        kwarn("waitpid on invalid process id: %zu", pid);
        return -NOT_FOUND;
    } 
    return block_waitpid(cur_task, child_index);
}
intptr_t sys_chdir(const char* path) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_chdir(\"%s\")", path);
#endif
    size_t pathlen = strlen(path);
    if(pathlen >= PATH_MAX) return -LIMITS; 
    intptr_t e;
    Process* cur_proc = current_process();
    Path p;
    if((e=parse_path(cur_proc, &p, path)) < 0) return e;
    Inode* inode;
    if((e=vfs_find(&p, &inode)) < 0) return e;
    if(inode->kind != INODE_DIR) {
        idrop(inode);
        return -IS_NOT_DIRECTORY;
    }
    switch(path[0]) {
    case '/':
         if(pathlen >= PATH_MAX) {
             e=-LIMITS;
             goto str_path_resolve_err;
         }
         memcpy(cur_proc->curdir, path, pathlen+1);
         break;
    case '.':
         path++;
         if(path[0] != '/') {
             e=-INVALID_PATH;
             goto str_path_resolve_err;
         } 
         path++;
         pathlen-=2;
    default: {
         size_t cwdlen = strlen(cur_proc->curdir);
         if(cwdlen+pathlen+1 >= PATH_MAX) {
             e=-LIMITS;
             goto str_path_resolve_err;
         }
         if(cwdlen == 1) cwdlen--;
         cur_proc->curdir[cwdlen] = '/';
         memcpy(cur_proc->curdir+cwdlen+1, path, pathlen+1);
    } break;
    }
    idrop(cur_proc->curdir_inode);
    cur_proc->curdir_inode = inode;
    return 0;
str_path_resolve_err:
    return e;
}
intptr_t sys_getcwd(char* buf, size_t cap) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_getcwd(%p, %zu)", buf, cap);
#endif
    if(cap == 0) return -INVALID_PARAM;
    Process* cur_proc = current_process();
    size_t pathlen = strlen(cur_proc->curdir);
    size_t amount = pathlen < cap-1 ? pathlen : cap-1;
    memcpy(buf, cur_proc->curdir, amount);
    buf[amount] = '\0';
    return 0;
}
intptr_t sys_stat(const char* path, Stats* stats) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_stat(%s, %p)", path, stats);
#endif
    Path p;
    Process* current = current_process();
    intptr_t e = 0;
    if((e=parse_path(current, &p, path)) < 0) return e;
    Inode* inode;
    if((e=vfs_find(&p, &inode)) < 0) return e;
    e=inode_stat(inode, stats);
    idrop(inode);
    return e;
}
void sys_sleepfor(const MinOS_Duration* duration) {
    size_t ms = duration->secs*1000 + duration->nano/1000000;
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_sleepfor(%zu)", ms);
#endif
    if(ms == 0) return;
    size_t until = system_timer_milis() + ms;
    return block_sleepuntil(current_task(), until); 
}
intptr_t sys_gettime(MinOS_Time* time) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_gettime(%p)", time);
#endif
    // TODO: Actual gettime with EPOCH (1/1/1970)
    time->ms = system_timer_milis();
    return 0;
}
intptr_t sys_truncate(uintptr_t handle, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_truncate(%lu, %zu)", handle, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    return inode_truncate(res->inode, size);
}
// TODO: strace
intptr_t sys_epoll_create1(int flags) {
    // TODO: EPOLL_CLOEXEC I guess
    (void)flags;
    size_t id = 0;
    Process* current = current_process();
    Resource* resource = resource_add(current->resources, &id);
    if(!resource) return -NOT_ENOUGH_MEM;
    resource->inode = (Inode*)epoll_new();
    if(!resource->inode) {
        resource_remove(current->resources, id);
        return -NOT_ENOUGH_MEM;
    }
    return id;
}
// TODO: strace
intptr_t sys_epoll_ctl(int epfd, int op, int fd, const struct epoll_event *event) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_epoll_ctl(%d, %d, %d, %p)", epfd, op, fd, event);
#endif
    Process* current = current_process();
    if(epfd < 0 || fd < 0) return -INVALID_HANDLE;
    Resource* res = resource_find_by_id(current->resources, epfd);
    if(!res) return -INVALID_HANDLE;
    if(res->inode->kind != INODE_EPOLL) return -INVALID_TYPE;
    switch(op) {
    case EPOLL_CTL_ADD: {
        EpollFd* entry = epoll_fd_new(fd, event);
        if(!entry) return -NOT_ENOUGH_MEM;
        intptr_t e = epoll_add((Epoll*)res->inode, entry);
        if(e < 0) epoll_fd_delete(entry);
        return e;
    } break;
    case EPOLL_CTL_MOD: {
        return epoll_mod((Epoll*)res->inode, fd, event);
    } break;
    case EPOLL_CTL_DEL: {
        return epoll_del((Epoll*)res->inode, fd);
    } break;
    }
    return -INVALID_PATH;
}
// TODO: strace
intptr_t sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    if(maxevents < 0 || epfd < 0) return -INVALID_PARAM;
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, epfd);
    if(!res) return -INVALID_HANDLE;
    if(res->inode->kind != INODE_EPOLL) return -INVALID_TYPE;
    Epoll* epoll = (Epoll*)res->inode;
    if(timeout == 0) epoll_poll(epoll, current);
    else {
        size_t until = 0xFFFFFFFFFFFFFFFFL;
        if(timeout > 0) until = system_timer_milis() + timeout;
        block_epoll(current_task(), epoll, until);
    }
    size_t event_count = 0;
    struct list *next;
    for(struct list *head = epoll->ready.next; head != &epoll->ready && event_count < (size_t)maxevents; head = next) {
        next = head->next;
        EpollFd* entry = (EpollFd*)head;
        size_t i = event_count++;
        events[i].events = entry->result_events;
        events[i].data = entry->event.data;
        list_remove(head);
        list_insert(head, &epoll->unready);
    }
    return event_count;
}
// TODO: strace
intptr_t sys_socket(uint32_t domain, uint32_t type, uint32_t prototype) {
    (void)prototype; // <- prototype is unused
    if(domain >= _AF_COUNT) return -INVALID_PARAM;
    if(type != SOCK_STREAM) return -UNSUPPORTED_SOCKET_TYPE;
    SocketFamily* family = &socket_families[domain];
    if(!family->init) return -UNSUPPORTED_DOMAIN;
    Process* current = current_process();
    size_t id;
    Resource* res = resource_add(current->resources, &id);
    if(!res) return -NOT_ENOUGH_MEM;
    res->inode = new_inode();
    if(!res->inode) {
        resource_remove(current->resources, id);
        return -NOT_ENOUGH_MEM;
    }
    res->inode->kind = INODE_MINOS_SOCKET;
    res->flags = O_RDWR;
    intptr_t e = family->init(res->inode);
    if(e < 0) {
        idrop(res->inode);
        resource_remove(current->resources, id);
        return e;
    }
    return id;
}
// TODO: Completely remove these:
// TODO: strace
intptr_t sys_send(uintptr_t sockfd, const void *buf, size_t len) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    if(!(res->flags & O_NOBLOCK)) block_is_writeable(current_task(), res->inode);
    return inode_write(res->inode, buf, len, res->offset);
}
// TODO: strace
intptr_t sys_recv(uintptr_t sockfd,       void *buf, size_t len) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    if(!(res->flags & O_NOBLOCK)) block_is_readable(current_task(), res->inode);
    return inode_read(res->inode, buf, len, res->offset);
}

// TODO: strace
intptr_t sys_accept(uintptr_t sockfd, struct sockaddr* addr, size_t *addrlen) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    // TODO: verify addrlen + addr
    size_t id;
    Resource* result = resource_add(current->resources, &id);
    if(!result) return -NOT_ENOUGH_MEM;
    result->inode = new_inode();
    if(!result->inode) {
        resource_remove(current->resources, id);
        return -NOT_ENOUGH_MEM;
    }
    intptr_t e;
    if(!(res->flags & O_NOBLOCK)) block_is_readable(current_task(), res->inode);
    e = inode_accept(res->inode, result->inode, addr, addrlen);
    if(e < 0) {
        idrop(result->inode);
        resource_remove(current->resources, id);
        return e;
    }
    return id;
}

// TODO: strace
intptr_t sys_bind(uintptr_t sockfd, struct sockaddr* addr, size_t addrlen) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    return inode_bind(res->inode, addr, addrlen);
}
// TODO: strace
intptr_t sys_listen(uintptr_t sockfd, size_t n) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    return inode_listen(res->inode, n);
}
// TODO: strace
intptr_t sys_connect(uintptr_t sockfd, const struct sockaddr* addr, size_t addrlen) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    return inode_connect(res->inode, addr, addrlen);
}
// TODO: strace
intptr_t sys_shmcreate(size_t size) {
    size_t pages = (size + (PAGE_SIZE-1)) / PAGE_SIZE;
    if(pages == 0) return -INVALID_PARAM;
    SharedMemory* shm = cache_alloc(kernel.shared_memory_cache);
    if(!shm) return -NOT_ENOUGH_MEM;
    memset(shm, 0, sizeof(*shm));
    void* mem = kernel_malloc(PAGE_SIZE*pages);
    if(!mem) {
        cache_dealloc(kernel.shared_memory_cache, shm);
        return -NOT_ENOUGH_MEM;
    }
    shm->phys = (paddr_t)((uintptr_t)mem - KERNEL_MEMORY_MASK);
    shm->pages_count = pages;
    mutex_lock(&kernel.shared_memory_mutex);
    if(!ptr_darray_reserve(&kernel.shared_memory, 1)) {
        mutex_unlock(&kernel.shared_memory_mutex);
        shmdrop(shm);
        return -NOT_ENOUGH_MEM;
    }
    size_t n = ptr_darray_pick_empty_slot(&kernel.shared_memory);
    if(n == kernel.shared_memory.len) kernel.shared_memory.len++;
    kernel.shared_memory.items[n] = shm;
    mutex_unlock(&kernel.shared_memory_mutex);
    return n;
}
intptr_t sys_shmmap(size_t key, void** addr) {
    intptr_t e;
    mutex_lock(&kernel.shared_memory_mutex);
    if(key >= kernel.shared_memory.len  || kernel.shared_memory.items[key] == NULL) {
        e = -INVALID_HANDLE;
        goto err_invalid_handle;
    }
    SharedMemory* shm = kernel.shared_memory.items[key];
    MemoryRegion* region = memregion_new(
            KERNEL_PFLAG_USER | KERNEL_PTYPE_USER | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE,
            0,
            0
        );
    if(!region) {
        e = -NOT_ENOUGH_MEM;
        goto err_region;
    }
    MemoryList* list = memlist_new(region);
    if(!list) {
        e = -NOT_ENOUGH_MEM;
        goto err_list;
    }

    Task* cur_task = current_task();
    MemoryList* insert_into = memlist_find_available(&cur_task->memlist, region, (void*)cur_task->eoe, shm->pages_count, shm->pages_count);
    if(!insert_into) {
        e = -NOT_ENOUGH_MEM;
        goto err_memlist_find_available;
    }
    Process* cur_proc = current_process();
    mutex_lock(&cur_proc->shared_memory_mutex);
    if(!ptr_darray_reserve(&cur_proc->shared_memory, 1)) {
        e = -NOT_ENOUGH_MEM;
        goto err_shared_memory;
    }
    size_t n = ptr_darray_pick_empty_slot(&cur_proc->shared_memory);
    if(!page_mmap(cur_task->cr3, shm->phys, region->address, region->pages, region->pageflags)) {
        e = -NOT_ENOUGH_MEM; 
        goto err_page_mmap;
    }
    shm->shared++;

    if(n == cur_proc->shared_memory.len) cur_proc->shared_memory.len++;
    cur_proc->shared_memory.items[n] = shm;
    invalidate_pages((void*)region->address, region->pages);
    list_append(&list->list, &insert_into->list);
    mutex_unlock(&cur_proc->shared_memory_mutex);
    mutex_unlock(&kernel.shared_memory_mutex);
    *addr = (void*)region->address;
    return 0;
err_page_mmap:
err_shared_memory:
    mutex_unlock(&cur_proc->shared_memory_mutex);
err_memlist_find_available:
    memlist_dealloc(list, NULL);
    goto err_region;
err_list:
    memregion_drop(region, NULL);
err_region:
err_invalid_handle:
    mutex_unlock(&kernel.shared_memory_mutex);
    return e;
}
intptr_t sys_shmrem(size_t key) {
    mutex_lock(&kernel.shared_memory_mutex);
    if(key >= kernel.shared_memory.len || kernel.shared_memory.items[key] == NULL) {
        mutex_unlock(&kernel.shared_memory_mutex);
        return -INVALID_HANDLE;
    }
    shmdrop(kernel.shared_memory.items[key]);
    kernel.shared_memory.items[key] = NULL;
    mutex_unlock(&kernel.shared_memory_mutex);
    return 0;
}

