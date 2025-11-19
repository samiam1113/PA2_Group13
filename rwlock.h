#ifndef RWLOCK_H
#define RWLOCK_H
#include "HashFunctions.h"
#include <pthread.h>

typedef struct rwlock_t {
    pthread_mutex_t lock;
    pthread_cond_t readers;
    pthread_cond_t writers;
    int active_readers;
    int active_writers;
    int waiting_writers;
} rwlock_t;

// Function prototypes
void rwlock_init(rwlock_t *rwlock);
void rwlock_acquire_readlock(rwlock_t *lock);
void rwlock_release_readlock(rwlock_t *lock);
void rwlock_acquire_writelock(rwlock_t *lock);
void rwlock_release_writelock(rwlock_t *lock);
void *reader(void *arg);
void *writer(void *arg);
//void rwlock_destroy(rwlock_t *rwlock);

#endif