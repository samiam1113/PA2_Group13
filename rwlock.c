#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "HashFunctions.c"

// prototypes from hashfunctions.c
void log_message(int priority, const char *message);
void action_msg(int priority, const char *action, uint32_t hash, const char *name, uint32_t salary);

// ========== RW Lock Implementation (from OSTEP) ==========
typedef struct _rwlock_t {
    sem_t writelock;
    sem_t lock;
    int readers;
} rwlock_t;

void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    sem_init(&rw->lock, 0, 1);      // Binary semaphore for protecting readers count
    sem_init(&rw->writelock, 0, 1); // Binary semaphore for write exclusion
}

void rwlock_acquire_readlock(rwlock_t *rw, int priority) {
    log_message(priority, "WAITING FOR READ LOCK");
    sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1) // First reader acquires write lock
        sem_wait(&rw->writelock);
    sem_post(&rw->lock);
    log_message(priority, "READ LOCK ACQUIRED");
}

void rwlock_release_readlock(rwlock_t *rw, int priority) {
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) // Last reader releases write lock
        sem_post(&rw->writelock);
    sem_post(&rw->lock);
    log_message(priority, "READ LOCK RELEASED");
}

void rwlock_acquire_writelock(rwlock_t *rw, int priority) {
    log_message(priority, "WAITING FOR WRITE LOCK");
    sem_wait(&rw->writelock);
    log_message(priority, "WRITE LOCK ACQUIRED");
}

void rwlock_release_writelock(rwlock_t *rw, int priority) {
    sem_post(&rw->writelock);
    log_message(priority, "WRITE LOCK RELEASED");
}

void rwlock_destroy(rwlock_t *rw) {
    sem_destroy(&rw->lock);
    sem_destroy(&rw->writelock);
}

rwlock_t *locks = NULL; 