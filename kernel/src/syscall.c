#include "syscall.h"
#include "print.h"
#include "timer.h"
#include "process.h"
#include "task.h"
#include "exec.h"
#include "log.h"
#include "benchmark.h"
#include "arch/x86_64/idt.h"
#include <minos/heap.h>
#include <minos/time.h>

void init_syscalls() {
    idt_register(0x80, syscall_base, IDT_SOFTWARE_TYPE);
}
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
#define strace(fmt, ...) ktrace("P%zu:T%zu "fmt, kernel.current_processid, kernel.current_taskid, __VA_ARGS__)
#define strace1(fmt)     ktrace("P%zu:T%zu "fmt, kernel.current_processid, kernel.current_taskid)
// TODO: Safety features like copy_to_userspace, copy_from_userspace
intptr_t sys_open(const char* path, fmode_t mode, oflags_t flags) {
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
    Inode* inode;
    if((e=vfs_find(&p, &inode)) < 0) {
        if(e == -NOT_FOUND && flags & O_CREAT) {
            // TODO: Consider maybe adding Inode** to creat
            if((e=vfs_creat(&p, flags & O_DIRECTORY)) < 0) return e;
            if((e=vfs_find(&p, &inode)) < 0) return e;
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
    resource->kind = RESOURCE_INODE;
    resource->as.inode.inode = inode;
    resource->as.inode.offset = 0;
    return id;
}
intptr_t sys_write(uintptr_t handle, const void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_write(%lu, %p, %zu)", handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    intptr_t e;
    if((e=inode_write(res->as.inode.inode, buf, size, res->as.inode.offset)) < 0) return e;
    res->as.inode.offset += e;
    return e;
}
intptr_t sys_read(uintptr_t handle, void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_read(%lu, %p, %zu)", handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) {
        return -INVALID_TYPE;
    }
    intptr_t e;
    // TODO: its now redily apparent that we should have a separate syscall for
    // get_dir_entries
    switch(res->as.inode.inode->kind) {
    case INODE_DEVICE:
    case INODE_FILE:
        if((e=inode_read(res->as.inode.inode, buf, size, res->as.inode.offset)) < 0) {
            return e;
        }
        res->as.inode.offset += e;
        return e;
    case INODE_DIR: {
        size_t read_bytes;
        if((e=inode_get_dir_entries(res->as.inode.inode, buf, size, res->as.inode.offset, &read_bytes)) < 0) {
            return e;
        }
        res->as.inode.offset += e;
        return read_bytes;
    } break;
    default:
        return -INVALID_TYPE;
    }
}

intptr_t sys_ioctl(uintptr_t handle, Iop op, void* arg) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_ioctl(%lu, %zu, %p)", handle, op, arg);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    return inode_ioctl(res->as.inode.inode, op, arg);
}

intptr_t sys_mmap(uintptr_t handle, void** addr, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_mmap(%lu, %p, %zu)", handle, addr, size);
#endif
    Process* current = current_process();
    Task* task = current_task();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    MmapContext context = {
        .page_table = task->image.cr3,
        .memlist = &task->image.memlist
    };
    intptr_t e = inode_mmap(res->as.inode.inode, &context, addr, size);
    if(e < 0) return e;
    invalidate_pages(*addr, PAGE_ALIGN_UP(size)/PAGE_SIZE);
    return e;
}
intptr_t sys_seek(uintptr_t handle, off_t offset, seekfrom_t from) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_seek(%lu, %d, %zu)", handle, (int)offset, from);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    if(res->as.inode.inode->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    // TODO: Convertions with lba
    switch(from) {
    case SEEK_START:
        res->as.inode.offset = offset;
        return res->as.inode.offset;
    case SEEK_CURSOR:
        res->as.inode.offset += offset;
        return res->as.inode.offset;
    case SEEK_EOF: {
        intptr_t e;
        if((e=inode_size(res->as.inode.inode)) < 0) return e;
        res->as.inode.offset = e+offset;
        return res->as.inode.offset;
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
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    if(res->as.inode.inode->kind != INODE_FILE) return -INODE_IS_DIRECTORY;
    return res->as.inode.offset;
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
    intptr_t e = 0;
    if(res->shared == 1) {
        e=0;
        switch(res->kind) {
        case RESOURCE_INODE:
            idrop(res->as.inode.inode);
            break;
        case RESOURCE_EPOLL:
            epoll_destroy(&res->as.epoll);
            break;
        }
    }
    resource_remove(current->resources, handle);
    return e;
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
    Heap* heap = (Heap*)current_proc->heap_list.next;
    while(&heap->list != &current_proc->heap_list) {
        Heap* nh = heap_clone(heap);
        if(!nh) {
            process_drop(process);
            return -NOT_ENOUGH_MEM;
        }
        list_append(&nh->list, &process->heap_list);
        heap = (Heap*)heap->list.next;
    }
    process->heapid = current_proc->heapid;
    process->curdir_inode = iget(current_proc->curdir_inode);

    Task* current = current_task();
    disable_interrupts();
    Task* task = kernel_task_add();
    if(!task) {
        process_drop(process);
        enable_interrupts();
        return -LIMITS;
    } 
    process->main_thread = task;
    task->process = process;

    if((e=fork_trampoline(current, task)) < 0) {
        process_drop(process);
        drop_task(task);
        enable_interrupts();
        return e;
    }
    enable_interrupts();
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
    disable_interrupts();
    Task* task = kernel_task_add();
    if(!task) {
        enable_interrupts();
        return -LIMITS;
    }
    task->process = cur_proc;
    Args args=create_args(argc, argv);
    Args env =create_args(envc, envv);
    Path p;
    if((e=parse_path(cur_proc, &p, path)) < 0) {
        drop_task(task);
        enable_interrupts();
        return e;
    }
    if((e=exec(task, &p, &args, &env)) < 0) {
        drop_task(task);
        enable_interrupts();
        return e;
    }
    cur_task->image.flags &= ~(TASK_FLAG_PRESENT);
    cur_task->image.flags |= TASK_FLAG_DYING;
    cur_proc->main_thread = task;
    task->image.flags |= TASK_FLAG_PRESENT;
    Heap* heap = (Heap*)cur_proc->heap_list.next;
    while(&heap->list != &cur_proc->heap_list) {
        Heap* next = (Heap*)heap->list.next;
        heap_destroy(heap);
        heap = next;
    }
    enable_interrupts();
    // TODO: thread yield
    for(;;) asm volatile("hlt");
    return 0;
}

void sys_exit(int code) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_exec(%d)", code);
#endif
    Process* cur_proc = current_process();
    Task* cur_task = current_task();
    Process* parent_proc = cur_proc->parent;
    disable_interrupts();
    cur_task->image.flags &= ~(TASK_FLAG_PRESENT);
    cur_task->image.flags |= TASK_FLAG_DYING;
    cur_proc->flags |= PROC_FLAG_DYING;
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
#define MIN_HEAP_PAGES 16
#define MAX_HEAP_PAGES 64
// FIXME: Possible problem with multiple tasks
intptr_t sys_heap_create(uint64_t flags, void* addr, size_t size_min) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_heap_create(%zu, %zu)", (size_t)flags, size_min);
#endif
    Process* cur_proc = current_process();
    Task* cur_task = current_task();
    MemoryRegion* region = memregion_new(
        MEMREG_WRITE,
        KERNEL_PFLAG_USER | KERNEL_PTYPE_USER | KERNEL_PFLAG_WRITE | KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_EXEC_DISABLE,
        0,
        0
    );
    if(!region) return -NOT_ENOUGH_MEM;
    MemoryList* list = memlist_new(region);
    if(!list) {
        memregion_drop(region, NULL);
        return -NOT_ENOUGH_MEM;
    }
    size_t pages_max = MAX_HEAP_PAGES;
    size_t pages_min = (size_min+(PAGE_SIZE-1))/PAGE_SIZE;
    if(!pages_min) pages_min = MIN_HEAP_PAGES;
    if(pages_max < pages_min) pages_max = pages_min;
    if(flags & HEAP_EXACT) pages_max = 0xFFFFFFFFFFFFFLL;
    // FIXME: Region must be shared between all tasks and must be available in all tasks
    MemoryList* insert_into = memlist_find_available(&cur_task->image.memlist, region, addr, pages_min, pages_max);
    if(!insert_into) {
        memlist_dealloc(list, NULL);
        return -NOT_ENOUGH_MEM;
    }
    if(flags & HEAP_EXACT) {
        if(region->address > (uintptr_t)addr || region->address + (region->pages*PAGE_SIZE) < ((uintptr_t)addr + pages_min * PAGE_SIZE)) {
            memlist_dealloc(list, NULL);
            return -VIRTUAL_SPACE_OCCUPIED;
        }
        region->address = (uintptr_t)addr;
        region->pages = pages_min;
    }
    Heap* heap = heap_new(cur_proc->heapid++, region->address, region->pages, flags);
    if(!heap) {
        memlist_dealloc(list, NULL);
        return -NOT_ENOUGH_MEM;
    }
    if(!page_alloc(cur_task->image.cr3, heap->address, heap->pages, region->pageflags)) {
        heap_destroy(heap);
        memlist_dealloc(list, NULL);
        return -NOT_ENOUGH_MEM;
    }
    invalidate_pages((void*)heap->address, heap->pages);
    list_append(&heap->list, cur_proc->heap_list.prev);
    list_append(&list->list, &insert_into->list);
    return heap->id;
}
intptr_t sys_heap_get(uintptr_t id, MinOSHeap* result) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_heap_get(%zu, %p)", id, result);
#endif
    Process* cur_proc = current_process();
    Heap* heap = get_heap_by_id(cur_proc, id);
    if(!heap) return -INVALID_HANDLE;
    result->address = (void*)heap->address;
    result->size = heap->pages*PAGE_SIZE;
    return 0;
}
// TODO: Maybe make this into sys_heap_resize instead
intptr_t sys_heap_extend(uintptr_t id, size_t extra_bytes) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_heap_extend(%zu, %zu)", (size_t)id, extra_bytes);
#endif
    size_t extra_pages = (extra_bytes+(PAGE_SIZE-1))/PAGE_SIZE;
    if(extra_pages == 0) return 0;
    Process* cur_proc = current_process();
    Heap* heap = get_heap_by_id(cur_proc, id);
    if(!heap) return -INVALID_HANDLE;
    intptr_t e = process_heap_extend(cur_proc, heap, extra_pages);
    if(e < 0) return e;
    invalidate_pages((void*)heap->address, heap->pages);
    return e;
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
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    return inode_truncate(res->as.inode.inode, size);
}
// TODO: strace
intptr_t sys_epoll_create1(int flags) {
    // TODO: EPOLL_CLOEXEC I guess
    (void)flags;
    size_t id = 0;
    Process* current = current_process();
    Resource* resource = resource_add(current->resources, &id);
    if(!resource) return -NOT_ENOUGH_MEM;
    resource->kind = RESOURCE_EPOLL;
    epoll_init(&resource->as.epoll);
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
    if(res->kind != RESOURCE_EPOLL) return -INVALID_TYPE;
    switch(op) {
    case EPOLL_CTL_ADD: {
        EpollFd* entry = epoll_fd_new(fd, event);
        if(!entry) return -NOT_ENOUGH_MEM;
        intptr_t e = epoll_add(&res->as.epoll, entry);
        if(e < 0) epoll_fd_delete(entry);
        return e;
    } break;
    case EPOLL_CTL_MOD: {
        return epoll_mod(&res->as.epoll, fd, event);
    } break;
    case EPOLL_CTL_DEL: {
        return epoll_del(&res->as.epoll, fd);
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
    if(res->kind != RESOURCE_EPOLL) return -INVALID_TYPE;
    Epoll* epoll = &res->as.epoll;
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
    res->as.inode.inode = new_inode();
    if(!res->as.inode.inode) {
        resource_remove(current->resources, id);
        return -NOT_ENOUGH_MEM;
    }
    res->as.inode.inode->kind = INODE_MINOS_SOCKET;
    res->kind = RESOURCE_INODE;
    intptr_t e = family->init(res->as.inode.inode);
    if(e < 0) {
        idrop(res->as.inode.inode);
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
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    if(!(res->flags & FFLAGS_NONBLOCK)) block_is_writeable(current_task(), res->as.inode.inode);
    return inode_write(res->as.inode.inode, buf, len, res->as.inode.offset);
}
// TODO: strace
intptr_t sys_recv(uintptr_t sockfd,       void *buf, size_t len) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    if(!(res->flags & FFLAGS_NONBLOCK)) block_is_readable(current_task(), res->as.inode.inode);
    return inode_read(res->as.inode.inode, buf, len, res->as.inode.offset);
}

// TODO: strace
intptr_t sys_accept(uintptr_t sockfd, struct sockaddr* addr, size_t *addrlen) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    // TODO: verify addrlen + addr
    size_t id;
    Resource* result = resource_add(current->resources, &id);
    if(!result) return -NOT_ENOUGH_MEM;
    result->kind = RESOURCE_INODE;
    result->as.inode.inode = new_inode();
    if(!result->as.inode.inode) {
        resource_remove(current->resources, id);
        return -NOT_ENOUGH_MEM;
    }
    intptr_t e;
    if(!(res->flags & FFLAGS_NONBLOCK)) block_is_readable(current_task(), res->as.inode.inode);
    e = inode_accept(res->as.inode.inode, result->as.inode.inode, addr, addrlen);
    if(e < 0) {
        idrop(result->as.inode.inode);
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
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    return inode_bind(res->as.inode.inode, addr, addrlen);
}
// TODO: strace
intptr_t sys_listen(uintptr_t sockfd, size_t n) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    return inode_listen(res->as.inode.inode, n);
}
// TODO: strace
intptr_t sys_connect(uintptr_t sockfd, const struct sockaddr* addr, size_t addrlen) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, sockfd);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    return inode_connect(res->as.inode.inode, addr, addrlen);
}
