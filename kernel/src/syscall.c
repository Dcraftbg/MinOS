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
        res->from.id         = process->curdir_id;
        res->from.superblock = process->curdir_sb;
        res->path = path;
        return 0;
    }
}
// TODO: Safety features like copy_to_userspace, copy_from_userspace
intptr_t sys_open(const char* path, fmode_t mode, oflags_t flags) {
#ifdef CONFIG_LOG_SYSCALLS
    kinfo("sys_open(\"%s\", %d, %d)", path, (int)mode, (int)flags);
#endif
    Path p;
    Process* current = current_process();
    size_t id = 0;
    intptr_t e = 0;
    if((e=parse_path(current, &p, path)) < 0) return e;
    Resource* resource = resource_add(current->resources, &id);
    if(!resource) return -NOT_ENOUGH_MEM;
    resource->kind = RESOURCE_FILE;
    if((e=vfs_open(&p, &resource->data.file, mode)) < 0) {
        if(flags & O_CREAT) {
            if((e=vfs_create(&p)) < 0) {
                resource_remove(current->resources, id);
                return e;
            }
            // TODO: Is it defined behaviour if the file is created but can't be opened?
            if((e=vfs_open(&p, &resource->data.file, mode)) < 0) {
                resource_remove(current->resources, id);
                return e;
            }
            return id;
        }
        resource_remove(current->resources, id);
        return e;
    }
    return id;
}
intptr_t sys_write(uintptr_t handle, const void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_write(%lu, %p, %zu)\n",handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    return vfs_write(&res->data.file, buf, size);
}
intptr_t sys_read(uintptr_t handle, void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_read(%lu, %p, %zu)\n", handle, buf, size);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    return vfs_read(&res->data.file, buf, size);
}

intptr_t sys_ioctl(uintptr_t handle, Iop op, void* arg) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_ioctl(%lu, %zu, %p)\n", handle, op, arg);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    return vfs_ioctl(&res->data.file, op, arg);
}

intptr_t sys_mmap(uintptr_t handle, void** addr, size_t size) {
    Process* current = current_process();
    Task* task = current_task();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    MmapContext context = {
        .page_table = task->image.cr3,
        .memlist = &task->image.memlist
    };
    intptr_t e = vfs_mmap(&res->data.file, &context, addr, size);
    if(e < 0) return e;
    // TODO: invlp the pages instead of this
    invalidate_full_page_table();
    return e;
}
intptr_t sys_seek(uintptr_t handle, off_t offset, seekfrom_t from) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    return vfs_seek(&res->data.file, offset, from);
}
intptr_t sys_tell(uintptr_t handle) {
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    return res->data.file.cursor;
}
// TODO: More generic close for everything including directories, networking sockets, etc. etc.
intptr_t sys_close(uintptr_t handle) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_close(%lu)\n",handle);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    intptr_t e = 0;
    if(res->shared == 1) {
        e = vfs_close(&res->data.file);
    }
    resource_remove(current->resources, handle);
    return e;
}
intptr_t sys_fork() {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_fork()\n");
#endif
    intptr_t e;
    Process* current_proc = current_process();
    // TODO: Maybe put in a separate function like:
    // process_clone_image()
    Process* process = kernel_process_add();
    if(!process) return -LIMITS; // Reached max tasks and/or we're out of memory
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
    process->curdir_id = current_proc->curdir_id;
    process->curdir_sb = current_proc->curdir_sb;

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
    printf("sys_exec(%s, %p, %zu)\n", path, argv, argc);
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

void sys_exit(int64_t code) {
    Process* cur_proc = current_process();
    Task* cur_task = current_task();
    disable_interrupts();
    cur_task->image.flags &= ~(TASK_FLAG_PRESENT);
    cur_task->image.flags |= TASK_FLAG_DYING;
    cur_proc->flags |= PROC_FLAG_DYING;
    cur_proc->exit_code = code;
    enable_interrupts();
    // TODO: thread yield
    for(;;) asm volatile("hlt");
}

intptr_t sys_waitpid(size_t pid) {
    for(;;) {
        Process* proc = get_process_by_id(pid);
        if(!proc) return -NOT_FOUND;
        if(proc->flags & PROC_FLAG_DYING) return proc->exit_code;
        // TODO: thread yield
        asm volatile("hlt");
    }
}
#define MIN_HEAP_PAGES 16
#define MAX_HEAP_PAGES 64
// FIXME: Possible problem with multiple tasks
intptr_t sys_heap_create(uint64_t flags) {
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
    // FIXME: Region must be shared between all tasks and must be available in all tasks
    MemoryList* insert_into = memlist_find_available(&cur_task->image.memlist, region, MIN_HEAP_PAGES, MAX_HEAP_PAGES);
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
    invalidate_full_page_table();
    list_append(&heap->list, cur_proc->heap_list.prev);
    list_append(&list->list, &insert_into->list);
    return heap->id;
}
intptr_t sys_heap_get(uintptr_t id, MinOSHeap* result) {
    Process* cur_proc = current_process();
    Heap* heap = get_heap_by_id(cur_proc, id);
    if(!heap) return -INVALID_HANDLE;
    result->address = (void*)heap->address;
    result->size = heap->pages*PAGE_SIZE;
    return 0;
}
// TODO: Maybe make this into sys_heap_resize instead
intptr_t sys_heap_extend(uintptr_t id, size_t extra_bytes) {
    size_t extra_pages = (extra_bytes+(PAGE_SIZE-1))/PAGE_SIZE;
    if(extra_pages == 0) return 0;
    Process* cur_proc = current_process();
    Heap* heap = get_heap_by_id(cur_proc, id);
    if(!heap) return -INVALID_HANDLE;
    return process_heap_extend(cur_proc, heap, extra_pages);
}
intptr_t sys_chdir(const char* path) {
    size_t pathlen = strlen(path);
    if(pathlen >= PATH_MAX) return -LIMITS; 
    intptr_t e;
    Process* cur_proc = current_process();
    Path p;
    inodeid_t curdir_id = cur_proc->curdir_id;
    Superblock* curdir_sb = cur_proc->curdir_sb;
    if((e=parse_path(cur_proc, &p, path)) < 0) return e;
    if((e=vfs_find(&p, &cur_proc->curdir_sb, &cur_proc->curdir_id)) < 0) return e;
    Inode* inode;
    if((e=fetch_inode(cur_proc->curdir_sb, cur_proc->curdir_id, &inode, MODE_READ)) < 0) return e;
    if(inode->kind != INODE_DIR) {
        idrop(inode);
        return -IS_NOT_DIRECTORY;
    }
    idrop(inode);
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
    return 0;
str_path_resolve_err:
    cur_proc->curdir_id = curdir_id;
    cur_proc->curdir_sb = curdir_sb;
    return e;
}
intptr_t sys_getcwd(char* buf, size_t cap) {
    if(cap == 0) return -INVALID_PARAM;
    Process* cur_proc = current_process();
    size_t pathlen = strlen(cur_proc->curdir);
    size_t amount = pathlen < cap-1 ? pathlen : cap-1;
    memcpy(buf, cur_proc->curdir, amount);
    buf[amount] = '\0';
    return 0;
}
