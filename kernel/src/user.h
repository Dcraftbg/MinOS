#pragma once
#include "process.h"
#include "task.h"
#include "page.h"
#include "string.h"
// TODO: Return error if page type does not equal user
// TODO: Make this more general and accept page_t pml4, and make this just a wrapper macro that calls it with task->cr3
// @return:
//  < 0 - failure
//    0 - success
intptr_t user_memcpy(Task* task, void* dst, const void* src, size_t size);
// TODO:
// void user_memcpy_unchecked(...);
// TODO:
// void user_memcpy_fast(...);
