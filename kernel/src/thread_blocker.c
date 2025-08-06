#include "thread_blocker.h"
#include "task.h"
#include "process.h"
#include "log.h"
#include "epoll.h"
#include "timer.h"

bool try_resolve_waitpid(ThreadBlocker* blocker, Task* task) {
    Process* proc = task->process; 
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
    Process* proc = task->process; 
    if(blocker->as.epoll.until <= system_timer_milis() || epoll_poll(blocker->as.epoll.epoll, proc)) return true;
    return false;

}
bool try_resolve_sleep_until(ThreadBlocker* blocker, Task* task) {
    return blocker->as.sleep.until <= system_timer_milis();
}
bool try_resolve_is_readable(ThreadBlocker* blocker, Task* task) {
    return inode_is_readable(blocker->as.inode);
}
bool try_resolve_is_writeable(ThreadBlocker* blocker, Task* task) {
    return inode_is_writeable(blocker->as.inode);
}
static void task_block(Task* task) {
    // TODO: thread yield
    while(task->flags & TASK_FLAG_BLOCKING) asm volatile("hlt");
}
void block_sleepuntil(Task* task, size_t until) {
    task->blocker.as.sleep.until = until;
    task->blocker.try_resolve = try_resolve_sleep_until;
    task->flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
int block_waitpid(Task* task, size_t child_index) {
    task->blocker.as.waitpid.child_index = child_index;
    task->blocker.try_resolve = try_resolve_waitpid;
    task->flags |= TASK_FLAG_BLOCKING;
    task_block(task);
    return task->blocker.as.waitpid.exit_code;
}
void block_epoll(Task* task, Epoll* epoll, size_t until) {
    task->blocker.as.epoll.epoll = epoll;
    task->blocker.as.epoll.until = until;
    task->blocker.try_resolve = try_resolve_epoll;
    task->flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
void block_is_readable(Task* task, Inode* inode) {
    task->blocker.as.inode = inode;
    task->blocker.try_resolve = try_resolve_is_readable;
    task->flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
void block_is_writeable(Task* task, Inode* inode) {
    task->blocker.as.inode = inode;
    task->blocker.try_resolve = try_resolve_is_writeable;
    task->flags |= TASK_FLAG_BLOCKING;
    task_block(task);
}
