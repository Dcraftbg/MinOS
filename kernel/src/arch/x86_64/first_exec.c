#include "gdt.h"
#include <stdint.h>
#include <stddef.h>
typedef struct {
    // TODO: we could maybe put args in r15->r12 and then do a little xchg before xoring
    uint64_t r15, r14, r13, r12, rbx, rbp, rip;

    uint64_t argc, argv, envp;
    uint64_t iret_rip;
    uint64_t iret_cs;
    uint64_t iret_rflags;
    uint64_t iret_rsp;
    uint64_t iret_ss;
} UserFrame;
extern void user_first_exec(void);
void* setup_user_first_exec(void* stack_page, uintptr_t user_entry,
    uintptr_t user_stack, uintptr_t argc, uintptr_t user_argv, uintptr_t user_envp) {
    stack_page = ((uint8_t*)stack_page) - sizeof(UserFrame);
    UserFrame* frame = (UserFrame*)stack_page;
    // Setup switch_to frame
    frame->r15 = frame->r14 = frame->r13 = frame->r12 = frame->rbx = frame->rbp = 0;
    frame->rip = (uintptr_t)user_first_exec;
    // Setup args
    frame->argc = argc;
    frame->argv = user_argv;
    frame->envp = user_envp;
    // Setup the iretq frame
    frame->iret_rip = user_entry;
    frame->iret_cs = GDT_USERCODE | 0x3;
    frame->iret_rflags  = 0x202;
    frame->iret_rsp = user_stack;
    frame->iret_ss = GDT_USERDATA | 0x3;
    return stack_page;
}
