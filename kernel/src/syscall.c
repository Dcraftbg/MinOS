#include "syscall.h"
#include "print.h"
#include "kernel.h"
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
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
    intptr_t e;
    if((e=inode_write(res->inode, buf, size, res->offset)) < 0) return e;
    res->offset += e;
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
    switch(res->inode->kind) {
    case INODE_DEVICE:
    case INODE_FILE:
        if((e=inode_read(res->inode, buf, size, res->offset)) < 0) {
            return e;
        }
        res->offset += e;
        return e;
    case INODE_DIR: {
        size_t read_bytes;
        if((e=inode_get_dir_entries(res->inode, buf, size, res->offset, &read_bytes)) < 0) {
            return e;
        }
        res->offset += e;
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
    return inode_ioctl(res->inode, op, arg);
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
    intptr_t e = inode_mmap(res->inode, &context, addr, size);
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
    if(res->kind != RESOURCE_INODE) return -INVALID_TYPE;
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
    intptr_t e = 0;
    if(res->shared == 1) {
        e=0;
        if(res->kind == RESOURCE_INODE) idrop(res->inode);
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
    process->parentid = current_proc->id;
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
    process->main_threadid = task->id;
    task->processid = process->id;

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
    assert(cur_task->processid     == cur_proc->id);
    assert(cur_proc->main_threadid == cur_task->id && "Exec on multiple threads is not supported yet");
    disable_interrupts();
    Task* task = kernel_task_add();
    if(!task) {
        enable_interrupts();
        return -LIMITS;
    }
    task->processid = cur_proc->id;
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
    cur_proc->main_threadid = task->id;
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
    Process* parent_proc = get_process_by_id(cur_proc->parentid);
    disable_interrupts();
    cur_task->image.flags &= ~(TASK_FLAG_PRESENT);
    cur_task->image.flags |= TASK_FLAG_DYING;
    cur_proc->flags |= PROC_FLAG_DYING;
    if(!parent_proc) {
        kwarn("Called exit on init process maybe? Thats kind of bad.");
        if(cur_proc->parentid != INVALID_PROCESS_ID) 
            kwarn(" parent_id : %zu", cur_proc->parentid);
        else 
            kwarn(" parent_id : INVALID");
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
intptr_t sys_heap_create(uint64_t flags, size_t size_min) {
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
    // FIXME: Region must be shared between all tasks and must be available in all tasks
    MemoryList* insert_into = memlist_find_available(&cur_task->image.memlist, region, pages_min, pages_max);
    if(!insert_into) {
        memlist_dealloc(list, NULL);
        return -NOT_ENOUGH_MEM;
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
    size_t until = kernel.pit_info.ticks + ms;
    return block_sleepuntil(current_task(), until); 
}
intptr_t sys_gettime(MinOS_Time* time) {
#ifdef CONFIG_LOG_SYSCALLS
    strace("sys_gettime(%p)", time);
#endif
    // TODO: Actual gettime with EPOCH (1/1/1970)
    time->ms = kernel.pit_info.ticks;
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
    return inode_truncate(res->inode, size);
}
