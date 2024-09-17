#include "syscall.h"
#include "print.h"
#include "kernel.h"
#include "task.h"
#include "exec.h"
#include "log.h"

void init_syscalls() {
    idt_register(0x80, syscall_base, IDT_SOFTWARE_TYPE);
}
// TODO: Safety features like copy_to_userspace, copy_from_userspace
intptr_t sys_open(const char* path, fmode_t mode) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_open(\"%s\", %u)\n",path,mode);
#endif
    Task* current = current_task();
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
    Task* current = current_task();
    Resource* res = resource_find_by_id(current->resources, handle);
    if(!res) return -INVALID_HANDLE;
    if(res->kind != RESOURCE_FILE) return -INVALID_TYPE;
    return vfs_write(&res->data.file, buf, size);
}
intptr_t sys_read(uintptr_t handle, void* buf, size_t size) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_read(%lu, %p, %zu)\n", handle, buf, size);
#endif
    Task* current = current_task();
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
    Task* current = current_task();
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
    Task* current = current_task();
    Task* task = kernel_task_add();
    if(!task) return -LIMITS; // Reached max tasks and/or we're out of memory
    if((e=fork_trampoline(current, task)) < 0) {
        drop_task(task);
        return e;
    }

    Task* now = current_task();
    if(now->id == task->id) {
        return -YOU_ARE_CHILD;
    }
    return task->id;
}
intptr_t sys_exec(const char* path, const char** argv, size_t argc) {
#ifdef CONFIG_LOG_SYSCALLS
    printf("sys_exec(%s, %p, %zu)\n", path, argv, argc);
#endif
    intptr_t e;
    Task* current = current_task();
    current->flags &= ~(KERNEL_PFLAG_PRESENT);
    Task* ntask = kernel_task_add();
    if (!ntask) return -LIMITS;
    resourceblock_dealloc(ntask->resources);
    ntask->resources = current->resources;
    if ((e=exec(ntask, path, create_args(argc, argv))) < 0) return e;
    // Not reachable
    return 0;
}
