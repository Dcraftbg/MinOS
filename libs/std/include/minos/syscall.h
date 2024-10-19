#pragma once
#include <stddef.h>
#include <stdint.h>
#define SYSCALL_ASM "int $0x80"
#define syscall0(num) \
({ \
    intptr_t __sys__result; \
    asm volatile ( \
        SYSCALL_ASM \
        : "=a" (__sys__result) \
        : "a" (num) \
        : "memory" \
    ); \
    __sys__result; \
})

#define syscall1(num, arg1) \
({ \
    intptr_t __sys_result; \
    asm volatile ( \
        SYSCALL_ASM \
        : "=a" (__sys_result) \
        : "a" (num), "D" (arg1) \
        : "memory" \
    ); \
    __sys_result; \
})


#define syscall2(num, arg1, arg2) \
({ \
    intptr_t __sys_result; \
    asm volatile ( \
        SYSCALL_ASM \
        : "=a" (__sys_result) \
        : "a" (num), "D" (arg1), "S" (arg2)\
        : "memory" \
    ); \
    __sys_result; \
})

#define syscall3(num, arg1, arg2, arg3) \
({ \
    intptr_t __sys_result; \
    asm volatile ( \
        SYSCALL_ASM \
        : "=a" (__sys_result) \
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3)\
        : "memory" \
    ); \
    __sys_result; \
})

#define syscall4(num, arg1, arg2, arg3, arg4) \
({ \
    intptr_t __sys_result; \
    asm volatile ( \
        SYSCALL_ASM \
        : "=a" (__sys_result) \
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3), "c" (arg4)\
        : "memory" \
    ); \
    __sys_result; \
})

#define syscall5(num, arg1, arg2, arg3, arg4, arg5) \
({ \
    intptr_t __sys_result; \
    register long r10 __asm__("r10") = arg5;\
    __asm__ volatile ( \
        SYSCALL_ASM \
        : "=a" (__sys_result) \
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3), "c" (arg4), "r" (r10) \
        : "memory" \
    ); \
    __sys_result; \
})
