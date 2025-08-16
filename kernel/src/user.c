#include "user.h"
#include "process.h"
#include "task.h"
#include "page.h"
#include "string.h"
#include <minos/status.h>

intptr_t user_memcpy(Task* task, void* dst, const void* buf, size_t size) {
    page_t pml4 = task->cr3;
    while(size) {
        uintptr_t phys = virt_to_phys(pml4, PAGE_ALIGN_DOWN((uintptr_t)dst));
        if(!phys) return -WOULD_SEGFAULT; // Segfault?
        void* page = (void*)(phys | KERNEL_MEMORY_MASK);
        uintptr_t offset = ((uintptr_t)dst - PAGE_ALIGN_DOWN((uintptr_t)dst));
        size_t cpysize = size >= PAGE_SIZE ? PAGE_SIZE-offset : size;
        memcpy(page+offset, buf, cpysize);
        dst += cpysize;
        buf += cpysize;
        size -= cpysize;
    }
    return 0;
}
