#pragma once
#include "mutex.h"
// TODO: Checkout the RwLock implementation in rust maybe?
// Currently its using a pretty dumb mechanism and I think rusts
// version with state and flags seems better.

// NOTE: Mechanism is an implementation of that from wikipedia:
// https://en.wikipedia.org/wiki/Readers–writer_lock
typedef struct {
    Mutex r;
    Mutex g;
    atomic_size_t b;
} RwLock;
#define rwlock_init(lock) \
    *(lock) = (RwLock){0}

#define rwlock_begin_read(lock)\
    do {\
        mutex_lock(&(lock)->r);\
        if(++(lock)->b == 1) mutex_lock(&(lock)->g);\
        mutex_unlock(&(lock)->r);\
    } while(0)

#define rwlock_end_read(lock)\
    do {\
        mutex_lock(&(lock)->r);\
        if(--(lock)->b == 0) mutex_unlock(&(lock)->g);\
        mutex_unlock(&(lock)->r);\
    } while(0)

#define rwlock_begin_write(lock)\
    do {\
        mutex_lock(&(lock)->g);\
    } while(0)

#define rwlock_end_write(lock)\
    do {\
        mutex_unlock(&(lock)->g);\
    } while(0)


