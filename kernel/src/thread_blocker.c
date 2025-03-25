#include "thread_blocker.h"
#include "task.h"
#include "process.h"
#include "log.h"
#include "epoll.h"

bool try_resolve_waitpid(ThreadBlocker* blocker, Task* task) {
    Process* proc = get_process_by_id(task->processid);
    debug_assert(blocker->as.waitpid.child_index < ARRAY_LEN(proc->children) && "Invalid child_index");
    if(child_process_is_dead(proc->children[blocker->as.waitpid.child_index])) {
        blocker->as.waitpid.exit_code = child_process_get_exit_code(proc->children[blocker->as.waitpid.child_index]);
        child_process_set_id(proc->children[blocker->as.waitpid.child_index], INVALID_PROCESS_ID);
        proc->children[blocker->as.waitpid.child_index].exit_code = 0;
        return true;
    }
    return false;
}
bool try_resolve_epoll(ThreadBlocker* blocker, Task* task) {
    Process* proc = get_process_by_id(task->processid);
    if(blocker->as.epoll.until <= kernel.pit_info.ticks || epoll_poll(blocker->as.epoll.epoll, proc)) return true;
    return false;

}
bool try_resolve_sleep_until(ThreadBlocker* blocker, Task* task) {
    return blocker->as.sleep.until <= kernel.pit_info.ticks;
}
// TODO: This will just be inode_is_readable
bool try_resolve_accept(ThreadBlocker* blocker, Task* task) {
    return (blocker->as.accept.e = inode_accept(blocker->as.accept.sock, blocker->as.accept.result, (struct sockaddr*)blocker->as.accept.addr_buf, &blocker->as.accept.addrlen)) != -WOULD_BLOCK;
}
static void task_block(Task* task) {
    // TODO: thread yield
    while(task->image.flags & TASK_FLAG_BLOCKING) asm volatile("hlt");
}
intptr_t block_accept(Task* task, Inode* sock, Inode* result, struct sockaddr* addr, size_t* addrlen) {
    if(addrlen && (task->image.blocker.as.accept.addrlen=(*addrlen)) > _sockaddr_max) return -LIMITS;
    memcpy(task->image.blocker.as.accept.addr_buf, addr, task->image.blocker.as.accept.addrlen);
    task->image.blocker.as.accept.sock = sock;
    task->image.blocker.as.accept.result = result;
    task->image.blocker.try_resolve = try_resolve_accept;
    task->image.flags |= TASK_FLAG_BLOCKING;
    task_block(task);
    if(addrlen) *addrlen = task->image.blocker.as.accept.addrlen;
    if(addr) memcpy(addr, task->image.blocker.as.accept.addr_buf, task->image.blocker.as.accept.addrlen);
    return task->image.blocker.as.accept.e;
}
void block_sleepuntil(Task* task, size_t until) {
    task->image.blocker.as.sleep.until = until;
    task->image.blocker.try_resolve = try_resolve_sleep_until;
    task->image.flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
int block_waitpid(Task* task, size_t child_index) {
    task->image.blocker.as.waitpid.child_index = child_index;
    task->image.blocker.try_resolve = try_resolve_waitpid;
    task->image.flags |= TASK_FLAG_BLOCKING;
    task_block(task);
    return task->image.blocker.as.waitpid.exit_code;
}
void block_epoll(Task* task, Epoll* epoll, size_t until) {
    task->image.blocker.as.epoll.epoll = epoll;
    task->image.blocker.as.epoll.until = until;
    task->image.blocker.try_resolve = try_resolve_epoll;
    task->image.flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
