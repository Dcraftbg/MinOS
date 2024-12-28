#include "thread_blocker.h"
#include "task.h"
#include "process.h"
#include "log.h"
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

bool try_resolve_sleep_until(ThreadBlocker* blocker, Task* task) {
    return blocker->as.sleep.until <= kernel.pit_info.ticks;
}

void block_sleepuntil(Task* task, size_t until) {
    task->image.flags |= TASK_FLAG_BLOCKING;
    task->image.blocker.as.sleep.until = until;
    task->image.blocker.try_resolve = try_resolve_sleep_until;
    // TODO: thread yield
    while(task->image.flags & TASK_FLAG_BLOCKING) asm volatile("hlt");
}
int block_waitpid(Task* task, size_t child_index) {
    task->image.flags |= TASK_FLAG_BLOCKING;
    task->image.blocker.as.waitpid.child_index = child_index;
    task->image.blocker.try_resolve = try_resolve_waitpid;
    // TODO: thread yield
    while(task->image.flags & TASK_FLAG_BLOCKING) asm volatile("hlt");
    return task->image.blocker.as.waitpid.exit_code;
}
