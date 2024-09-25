#pragma once
#include <stdatomic.h>
typedef atomic_bool Spinlock;
#define spinlock_lock(m) \
    do { \
      while(atomic_flag_test_and_set(m));\
    } while(0)

#define spinlock_unlock(m) \
    atomic_flag_clear(m)
