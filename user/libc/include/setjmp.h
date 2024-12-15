#pragma once
#include <stdint.h>
typedef uintptr_t jmp_buf[2];
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value);
