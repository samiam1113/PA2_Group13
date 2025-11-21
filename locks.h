#ifndef LOCKS_H
#define LOCKS_H

#include <semaphore.h>

// ========== RW Lock Implementation (from OSTEP) ==========
typedef struct _rwlock_t {
    sem_t writelock;
    sem_t lock;
    int readers;
} rwlock_t;

void log_message(int priority, const char *message);

void rwlock_init(rwlock_t *rw);

void rwlock_acquire_readlock(rwlock_t *rw, int priority);

void rwlock_release_readlock(rwlock_t *rw, int priority);

void rwlock_acquire_writelock(rwlock_t *rw, int priority);

void rwlock_release_writelock(rwlock_t *rw, int priority);

void rwlock_destroy(rwlock_t *rw);


#endif

