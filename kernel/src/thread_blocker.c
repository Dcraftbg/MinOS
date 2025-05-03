#include "thread_blocker.h"
#include "task.h"
#include "process.h"
#include "log.h"
#include "epoll.h"

bool try_resolve_waitpid(ThreadBlocker* blocker, Task* task) {
    if(mutex_try_lock(&kernel.processes_mutex)) return false;
    debug_assert(task->processid < kernel.processes.len);
    Process* proc = kernel.processes.items[task->processid];
    mutex_unlock(&kernel.processes_mutex);
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
    debug_assert(task->processid < kernel.processes.len);
    Process* proc = kernel.processes.items[task->processid];
    mutex_unlock(&kernel.processes_mutex);
    if(blocker->as.epoll.until <= kernel.pit_info.ticks || epoll_poll(blocker->as.epoll.epoll, proc)) return true;
    return false;

}
bool try_resolve_sleep_until(ThreadBlocker* blocker, Task* task) {
    return blocker->as.sleep.until <= kernel.pit_info.ticks;
}
bool try_resolve_is_readable(ThreadBlocker* blocker, Task* task) {
    return inode_is_readable(blocker->as.inode);
}
bool try_resolve_is_writeable(ThreadBlocker* blocker, Task* task) {
    return inode_is_writeable(blocker->as.inode);
}
static void task_block(Task* task) {
    // TODO: thread yield
    while(task->image.flags & TASK_FLAG_BLOCKING) asm volatile("hlt");
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
void block_is_readable(Task* task, Inode* inode) {
    task->image.blocker.as.inode = inode;
    task->image.blocker.try_resolve = try_resolve_is_readable;
    task->image.flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
void block_is_writeable(Task* task, Inode* inode) {
    task->image.blocker.as.inode = inode;
    task->image.blocker.try_resolve = try_resolve_is_writeable;
    task->image.flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
