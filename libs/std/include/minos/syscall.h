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
