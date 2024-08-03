#pragma once
#include <stdatomic.h>
typedef atomic_bool Mutex;
// TODO: thread_yield
//       ^ will require thread yield syscall
#define mutex_lock(m) \
    do { \
      while(atomic_flag_test_and_set(m));\
    } while(0)

#define mutex_unlock(m) \
    atomic_flag_clear(m)
