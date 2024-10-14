#include "syscall.h"
#include "print.h"
#include "kernel.h"
#include "process.h"
#include "task.h"
#include "exec.h"
#include "log.h"
#include "benchmark.h"
#include "arch/x86_64/idt.h"

void init_syscalls() {
    idt_register(0x80, syscall_base, IDT_SOFTWARE_TYPE);
}
// TODO: Safety features like copy_to_userspace, copy_from_userspace
intptr_t sys_open(const char* path, fmode_t mode) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_open(\"%s\", %u)\n",path,mode);
#endif
    Process* current = current_process();
    size_t id = 0;
    intptr_t e = 0;
    Resource* resource = resource_add(current->resources, &id);
    if(!resource) return -NOT_ENOUGH_MEM;
    resource->kind = RESOURCE_FILE;
    if((e=vfs_open(path, &resource->data.file, mode)) < 0) {
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
// TODO: More generic close for everything including directories, networking sockets, etc. etc.
intptr_t sys_close(uintptr_t handle) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_close(%lu)\n",handle);
#endif
    Process* current = current_process();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    intptr_t e = vfs_close(&res->data.file);
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
intptr_t sys_exec(const char* path, const char** argv, size_t argc) {
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
    if((e=exec(task, path, create_args(argc, argv))) < 0) {
        drop_task(task);
        enable_interrupts();
        return e;
    }
    cur_task->image.flags &= ~(TASK_FLAG_PRESENT);
    cur_task->image.flags |= TASK_FLAG_DYING;
    cur_proc->main_threadid = cur_task->id;
    task->image.flags |= TASK_FLAG_PRESENT;
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
intptr_t sys_heap_create() {
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
    Heap* heap = heap_new(cur_proc->heapid++, region->address, region->pages);
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

intptr_t sys_heap_allocate(size_t id, size_t size, void** result) {
    Process* cur_proc = current_process();
    Heap* heap = get_heap_by_id(cur_proc, id);
    if(!heap) return -INVALID_HANDLE;
    void* alloc = heap_allocate(heap, size);
    if(alloc) {
        *result = alloc;
        return 0;
    }
    return -NOT_ENOUGH_MEM;
}

intptr_t sys_heap_deallocate(size_t id, void* addr) {
    Process* cur_proc = current_process();
    Heap* heap = get_heap_by_id(cur_proc, id);
    if(!heap) return -INVALID_HANDLE;
    heap_deallocate(heap, addr);
    return 0;
}
